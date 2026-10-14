// The only cache mutation policy. Called by protected workflows and test-ci.py.
const fs = require('node:fs');
const crypto = require('node:crypto');
const policy = require('../migration/cache-policy.json');
const MiB = 1024 ** 2;
const protectedRef = 'refs/heads/rust-migration';
const slotKey = (family, slot) => `bounded-v1-${family}-${slot}`;
const pathFor = family => `.local/cache-payload/${family}.tgz`;
// actions/cache 0057852 (@actions/cache 4.1.0): fixed relative input, zstd,
// enableCrossOsArchive:true and version salt 1.0. Never vary these for writers.
const versionFor = family => crypto.createHash('sha256')
  .update(`${pathFor(family)}|zstd-without-long|1.0`).digest('hex');

function familyFor(key) {
  const slot = /^bounded-v1-(.+)-[01]$/.exec(key);
  if (slot) {
    if (!(slot[1] in policy.families)) throw Error(`Unknown bounded family: ${key}`);
    return slot[1];
  }
  if (/^compiler-v\d+-/.test(key)) return 'native';
  const linux = /^ccache-linux-v\d+-gcc-([^-]+)-/.exec(key);
  if (linux) {
    if (linux[1] === 'e3b0c44298fc') return 'linux-sdl2';
    if (linux[1] === 'b7a8fb837a74') return 'linux-dedicated';
  }
  const mac = /^ccache-macos-v\d+-arm64-.+-(a763a3567032|5c653c0e7a33)-/.exec(key);
  if (mac) {
    if (mac[1] === 'a763a3567032') return 'macos-debug';
    if (mac[1] === '5c653c0e7a33') return 'macos-release';
  }
  const msvc = /^ccache-msvc-v\d+-(x86|x64)-/.exec(key);
  if (msvc) return `msvc-${msvc[1]}`;
  if (/^(bounded-|compiler-|ccache-)/.test(key)) {
    throw Error(`Unclassified compiler generation: ${key}`);
  }
  return null;
}

function inventory(caches) {
  const groups = Object.fromEntries(Object.keys(policy.families).map(key => [key, []]));
  let other = 0;
  const ids = new Set();
  for (const cache of caches) {
    if (!Number.isSafeInteger(cache.id) || cache.id <= 0 || ids.has(cache.id) ||
        !['key', 'ref', 'version'].every(field => typeof cache[field] === 'string' && cache[field].length > 0) ||
        !Number.isSafeInteger(cache.size_in_bytes) || cache.size_in_bytes <= 0) {
      throw Error('Ambiguous cache inventory identity/size');
    }
    ids.add(cache.id);
    const family = familyFor(cache.key);
    if (!family) { other += cache.size_in_bytes; continue; }
    if (cache.ref !== protectedRef || cache.size_in_bytes > policy.families[family] * MiB) {
      throw Error(`Cache ref/size violates ${family} envelope: ${cache.key}`);
    }
    if (cache.key.startsWith('bounded-') && cache.version !== versionFor(family)) {
      throw Error(`Unexpected slot cache version: ${cache.key}`);
    }
    groups[family].push(cache);
  }
  if (other > policy.other_mib * MiB) throw Error('Restore-only caches exceed other-cache allowance');
  for (const [family, entries] of Object.entries(groups)) {
    if (entries.length > 2 || new Set(entries.map(entry => entry.key)).size !== entries.length) {
      throw Error(`More than retained+replacement or duplicate versions in ${family}`);
    }
    // Legacy transition always uses slot 0; slot 1 is inadmissible while a
    // legacy entry remains. Root must drain all earlier unbounded writers.
    if (entries.some(entry => !entry.key.startsWith('bounded-')) &&
        entries.some(entry => entry.key === slotKey(family, 1))) {
      throw Error(`Unreconciled legacy/slot transition in ${family}`);
    }
  }
  return {groups, other, bytes: caches.reduce((sum, cache) => sum + cache.size_in_bytes, 0)};
}

async function compare(github, context, base, head) {
  if (base === head) return 'identical';
  const {data} = await github.rest.repos.compareCommitsWithBasehead({
    ...context.repo, basehead: `${base}...${head}`
  });
  if (!['ahead', 'behind', 'identical'].includes(data.status)) {
    throw Error('Protected cache commits have ambiguous ancestry');
  }
  return data.status;
}

async function control({github, context, core, family, operation, read, write}) {
  if (context.eventName !== 'push' || context.ref !== protectedRef ||
      !/^[0-9a-f]{40}$/.test(context.sha) || !(family in policy.families)) {
    throw Error('Cache mutations require a fixed protected push SHA');
  }
  const list = () => github.paginate(github.rest.actions.getActionsCacheList, {
    ...context.repo, per_page: 100
  });
  let caches = await list();
  let snapshot = inventory(caches);
  let entries = snapshot.groups[family];
  core.info(`Cache list: ${snapshot.bytes} bytes; family ${family}: ${entries.length} entries`);
  const slots = [0, 1].map(slot => read(`${slot}.json`));
  // REST and actual restore must agree. Stale/partial lists never free a slot.
  for (let slot = 0; slot < 2; slot++) {
    const cache = entries.find(entry => entry.key === slotKey(family, slot));
    const payload = slots[slot];
    if (Boolean(cache) !== Boolean(payload) || (payload && (payload.key !== cache.key ||
        payload.schema !== 1 || payload.family !== family || !/^[0-9a-f]{40}$/.test(payload.sha) ||
        !/^[0-9a-f]{64}$/.test(payload.hash) || !/^[0-9a-f]{64}$/.test(payload.content_hash) || !Number.isSafeInteger(payload.bytes) ||
        payload.bytes <= 0 || payload.bytes + Math.ceil(payload.bytes / 100) + MiB > policy.families[family] * MiB))) {
      throw Error('REST slot metadata disagrees with restored payload');
    }
  }
  // A cancelled transition can leave legacy fallback after slot 0 commits.
  // Its verified manifest remembers predecessor IDs, so an omitted/stale list
  // cannot silently free a third slot. Retry only known own-family legacy IDs.
  if (operation === 'prepare') {
    for (const payload of slots.filter(Boolean)) {
      for (const legacy of payload.legacy || []) {
        if (familyFor(legacy.key) !== family || legacy.key.startsWith('bounded-') ||
            legacy.ref !== protectedRef || !Number.isSafeInteger(legacy.id) || legacy.id <= 0 ||
            typeof legacy.version !== 'string' || legacy.version.length === 0 ||
            !/-[0-9a-f]{40}$/.test(legacy.key)) throw Error('Invalid legacy predecessor identity');
        const listed = caches.find(cache => cache.id === legacy.id);
        if (listed) {
          if (listed.key !== legacy.key || listed.version !== legacy.version || listed.ref !== legacy.ref) {
            throw Error('Known legacy predecessor metadata changed');
          }
          continue;
        }
        const sha = legacy.key.slice(-40);
        if ((await compare(github, context, sha, payload.sha)) === 'behind') {
          throw Error('Legacy predecessor is newer than its verified replacement');
        }
        try {
          const response = await github.rest.actions.deleteActionsCacheById({...context.repo, cache_id: legacy.id});
          if (response.status !== 204) throw Error('Unconfirmed legacy reconciliation');
        } catch (error) {
          // Authenticated cache-list access succeeded; 404 confirms this known
          // committed ID was already removed. Other failures remain ambiguous.
          if (error.status !== 404) throw error;
        }
        const before = entries.map(cache => cache.id).sort().join(',');
        caches = await list(); snapshot = inventory(caches); entries = snapshot.groups[family];
        if (entries.map(cache => cache.id).sort().join(',') !== before) {
          throw Error('Legacy reconciliation changed visible family metadata');
        }
      }
    }
  }
  const shaFor = cache => {
    if (cache.key === slotKey(family, 0)) return slots[0].sha;
    if (cache.key === slotKey(family, 1)) return slots[1].sha;
    const match = /-([0-9a-f]{40})$/.exec(cache.key);
    if (!match) throw Error('Legacy cache has no protected commit identity');
    return match[1];
  };
  const remove = async cache => {
    const response = await github.rest.actions.deleteActionsCacheById({...context.repo, cache_id: cache.id});
    if (response.status !== 204) throw Error(`Unconfirmed deletion of cache ${cache.id}`);
    core.info(`Deleted confirmed predecessor ${cache.id}: ${cache.key}`);
  };
  if (operation === 'prepare') {
    let retained = entries[0];
    if (entries.length === 2) {
      const ordering = await compare(github, context, shaFor(entries[0]), shaFor(entries[1]));
      if (ordering === 'ahead' || (ordering === 'identical' && entries[1].key.startsWith('bounded-'))) {
        retained = entries[1];
      }
      // A legacy body has no content manifest. It is retained unless a new
      // fixed slot with at least the same protected ancestry is verified.
      const obsolete = entries.find(entry => entry.id !== retained.id);
      if (!retained.key.startsWith('bounded-')) throw Error('Legacy transition needs a verified replacement');
      await remove(obsolete);
      const reconciled = inventory(await list()).groups[family];
      if (reconciled.length !== 1 || reconciled[0].id !== retained.id) {
        throw Error('Cache deletion not visible; stop this family publication');
      }
      entries = reconciled;
    }
    if (retained && (await compare(github, context, context.sha, shaFor(retained))) === 'ahead') {
      core.setOutput('publish', 'false');
      core.info('A newer protected commit owns this family; keep its cache');
      return;
    }
    const target = retained?.key === slotKey(family, 0) ? 1 : 0;
    const legacy = entries.filter(cache => !cache.key.startsWith('bounded-')).map(({id, key, ref, version, size_in_bytes}) => ({id, key, ref, version, size_in_bytes}));
    const state = {family, sha: context.sha, target, predecessor: retained || null, legacy};
    write('publication.json', state);
    core.setOutput('key', slotKey(family, target));
    core.setOutput('slot', String(target));
    core.setOutput('publish', 'true');
  } else if (operation === 'finish') {
    const state = read('publication.json');
    const upload = read('upload.json');
    const outer = read('outer.json');
    if (!state || state.family !== family || state.sha !== context.sha || !upload || upload.sha !== context.sha) {
      throw Error('Missing exact protected publication state');
    }
    const target = entries.find(entry => entry.key === slotKey(family, state.target));
    if (!target || !outer || outer.sha !== context.sha || outer.cap !== policy.families[family] * MiB ||
        target.size_in_bytes !== outer.size || outer.size > outer.cap || slots[state.target]?.hash !== upload.hash) {
      throw Error('Upload ID/size/content unconfirmed; retain predecessor');
    }
    if (state.predecessor) {
      const predecessor = entries.find(entry => entry.id === state.predecessor.id);
      if (!predecessor || predecessor.key !== state.predecessor.key || predecessor.version !== state.predecessor.version) {
        throw Error('Predecessor metadata changed; stop publication');
      }
      await remove(predecessor);
    }
    const settled = inventory(await list()).groups[family];
    if (settled.length !== 1 || settled[0].id !== target.id) {
      throw Error('Publication prune not visible; stop this family');
    }
    core.info(`Confirmed ${target.id}: ${target.size_in_bytes} bytes, protected ${context.sha}`);
  } else {
    throw Error('Unknown cache policy operation');
  }
}

async function run({github, context, core}, operation, family) {
  const directory = `.local/cache-state/${family}`;
  const read = name => fs.existsSync(`${directory}/${name}`) ? JSON.parse(fs.readFileSync(`${directory}/${name}`, 'utf8')) : null;
  const write = (name, value) => fs.writeFileSync(`${directory}/${name}`, JSON.stringify(value));
  await control({github, context, core, operation, family, read, write});
}

module.exports = {control, run, inventory, familyFor, versionFor, slotKey, policy};
