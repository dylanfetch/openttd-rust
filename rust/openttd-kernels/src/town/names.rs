/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete retry/uniqueness policy over synchronous live native rendering.
use super::{State, lifecycle::NameParams};
use crate::services::{ByteSpan, Services, TownHandle};
use std::{collections::BTreeSet, ffi::c_void, mem::MaybeUninit};

#[repr(C)]
pub struct Scratch {
    pub words: [MaybeUninit<usize>; 4],
}
type NameSet = BTreeSet<Vec<u8>>;
const _: () = assert!(std::mem::size_of::<NameSet>() <= std::mem::size_of::<Scratch>());
const _: () = assert!(std::mem::align_of::<NameSet>() <= std::mem::align_of::<Scratch>());
#[repr(C)]
pub struct Randomizer {
    _private: [u8; 0],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Cursor {
    pub town: *const TownHandle,
    pub index: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct World {
    pub params: extern "C" fn() -> NameParams,
    pub next_random: extern "C" fn(*mut Randomizer) -> u32,
    pub next_town: extern "C" fn(u32) -> Cursor,
    pub grf_valid: extern "C" fn(u32) -> bool,
    pub render: extern "C" fn(
        NameParams,
        u32,
        *mut c_void,
        extern "C" fn(*mut c_void, ByteSpan) -> bool,
    ) -> bool,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_names_init(scratch: *mut Scratch) {
    // SAFETY: Fresh stack storage has the size/alignment checked above.
    unsafe {
        scratch.cast::<NameSet>().write(NameSet::new());
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_names_destroy(scratch: *mut Scratch) {
    // SAFETY: The native scope destroys its single initialized owner exactly once.
    unsafe {
        std::ptr::drop_in_place(scratch.cast::<NameSet>());
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_names_insert(scratch: *mut Scratch, input: ByteSpan) {
    // SAFETY: The live producer bytes are copied before the native renderer ends.
    let bytes = unsafe { input.bytes() }.to_vec();
    // SAFETY: Serialized live name set; this mutation invokes no callback.
    unsafe {
        (*scratch.cast::<NameSet>()).insert(bytes);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_names_clear(scratch: *mut Scratch) {
    // SAFETY: Unique live stack owner; no mutation spans a renderer or progress call.
    unsafe {
        (*scratch.cast::<NameSet>()).clear();
    }
}
fn char_count(input: ByteSpan) -> usize {
    // SAFETY: Renderer supplies bytes through this callback; no callback during count.
    let bytes = unsafe { input.bytes() };
    let (mut count, mut at) = (0, 0);
    while at < bytes.len() {
        count += 1;
        at = crate::utf8::next(bytes, at);
    }
    count
}
struct Compare {
    wanted: ByteSpan,
}
extern "C" fn compare_name(context: *mut c_void, actual: ByteSpan) -> bool {
    // SAFETY: The caller keeps its stack context and both rendered strings live;
    // immediate byte comparison retains no borrow after this callback returns.
    let wanted = unsafe { (*context.cast::<Compare>()).wanted };
    unsafe { wanted.bytes() == actual.bytes() }
}
type TownOwner = extern "C" fn(*const TownHandle) -> *mut State;
struct Verify {
    world: World,
    owner: TownOwner,
    scratch: *mut Scratch,
}
#[allow(clippy::if_not_else)] // Retain the original scratch-first branch order.
extern "C" fn verify_name(context: *mut c_void, name: ByteSpan) -> bool {
    // SAFETY: Renderer is synchronous; the complete stack context remains live.
    let (w, s, scratch) = unsafe {
        let p = context.cast::<Verify>();
        ((*p).world, (*p).owner, (*p).scratch)
    };
    if char_count(name) >= 32 {
        return false;
    }
    if !scratch.is_null() {
        // SAFETY: Live unique scratch. End the set/byte borrows before returning
        // through the native renderer; this branch invokes no other service.
        let set = unsafe { &mut *scratch.cast::<NameSet>() };
        let bytes = unsafe { name.bytes() };
        if set.contains(bytes) {
            return false;
        }
        set.insert(bytes.to_vec());
    } else {
        let mut cursor = (w.next_town)(0);
        while !cursor.town.is_null() {
            let owner = s(cursor.town);
            // SAFETY: Iterator supplied a live owner. Copying the empty fact ends
            // its borrow before GRF lookup/rendering, which may reenter Rust.
            let custom_empty = unsafe { (*owner).name.is_empty() };
            if custom_empty {
                // SAFETY: Copied constructor inputs, observed before GRF lookup.
                let (grfid, kind) = unsafe {
                    (
                        (*owner).simulation.townnamegrfid,
                        (*owner).simulation.townnametype,
                    )
                };
                let params = if grfid != 0 && !(w.grf_valid)(grfid) {
                    NameParams {
                        grfid: 0,
                        kind: 0x20c0,
                    }
                } else {
                    NameParams { grfid, kind }
                };
                // SAFETY: Source reads parts only after completing its constructor.
                let parts = unsafe { (*owner).simulation.townnameparts };
                let mut comparison = Compare { wanted: name };
                if (w.render)(params, parts, (&raw mut comparison).cast(), compare_name) {
                    return false;
                }
            } else {
                // SAFETY: Both inputs remain live; this immediate comparison invokes
                // no service and ends before the next pool operation.
                let same = unsafe { (*owner).name.as_slice() == name.bytes() };
                if same {
                    return false;
                }
            }
            cursor = (w.next_town)(u32::from(cursor.index) + 1);
        }
    }
    true
}
fn verify(
    world: World,
    owner: TownOwner,
    parts: u32,
    params: NameParams,
    scratch: *mut Scratch,
) -> bool {
    let mut context = Verify {
        world,
        owner,
        scratch,
    };
    (world.render)(params, parts, (&raw mut context).cast(), verify_name)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_verify_name(
    world: *const World,
    shared: *const Services,
    parts: u32,
    params: NameParams,
    scratch: *mut Scratch,
) -> bool {
    // SAFETY: Immutable native tables and optional live stack set outlive the call.
    let (w, s) = unsafe { (*world, (*shared).town_owner) };
    verify(w, s, parts, params, scratch)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_generate_name(
    world: *const World,
    shared: *const Services,
    randomizer: *mut Randomizer,
    output: *mut u32,
    scratch: *mut Scratch,
) -> bool {
    // SAFETY: Native tables/randomizer/output and optional scratch are live.
    let (w, s) = unsafe { (*world, (*shared).town_owner) };
    let params = (w.params)();
    for _ in 0..1000 {
        let parts = (w.next_random)(randomizer);
        if verify(w, s, parts, params, scratch) {
            // SAFETY: Native output is written at the original success point only.
            unsafe {
                *output = parts;
            }
            return true;
        }
    }
    false
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_unique_name(
    world: *const World,
    shared: *const Services,
    name: ByteSpan,
) -> bool {
    // SAFETY: Immutable table copies and borrowed input outlive this entry.
    let (w, s) = unsafe { (*world, (*shared).town_owner) };
    let mut cursor = (w.next_town)(0);
    while !cursor.town.is_null() {
        let owner: *mut State = s(cursor.town);
        // SAFETY: Immediate field/byte comparison ends before next pool callback.
        let duplicate =
            unsafe { !(*owner).name.is_empty() && (*owner).name.as_slice() == name.bytes() };
        if duplicate {
            return false;
        }
        cursor = (w.next_town)(u32::from(cursor.index) + 1);
    }
    true
}
