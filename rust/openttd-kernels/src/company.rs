/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
//! Company finance/history and complete economy lifecycle ownership.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::many_single_char_names,
    clippy::struct_field_names,
    clippy::verbose_bit_mask
)]
use std::cell::Cell;
use std::future::Future;
use std::pin::Pin;
use std::ptr::{addr_of, addr_of_mut};
use std::rc::Rc;
use std::task::{Context, Poll, Waker};
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Entry {
    pub income: i64,
    pub expenses: i64,
    pub cargo: [u32; 64],
    pub performance: i32,
    pub value: i64,
}
#[repr(C)]
pub struct Finances {
    pub money: i64,
    pub fraction: u8,
    pub loan: i64,
    pub max_loan: i64,
    pub preview: u8,
    pub empty: u8,
    pub bankruptcy: u8,
    pub asked: u16,
    pub timeout: i16,
    pub bankrupt_value: i64,
    pub terraform: u32,
    pub clear: u32,
    pub tree: u32,
    pub object: u32,
    pub expenses: [[i64; 13]; 3],
    pub current: Entry,
    pub old: [Entry; 24],
    pub valid: u8,
}
#[repr(C)]
pub struct Economy {
    pub max_loan: i64,
    pub fluct: i16,
    pub interest: u8,
    pub infl_amount: u8,
    pub infl_payment: u8,
    pub industry_counter: u32,
    pub industry_increment: u32,
    pub prices: u64,
    pub payment: u64,
    pub old_loan: i64,
    pub old_fraction: u16,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Action {
    pub kind: u32,
    pub id: u32,
    pub a: i64,
    pub b: i64,
    pub c: i64,
    pub d: i64,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub next: unsafe extern "C" fn(u32, u32) -> u32,
    pub read: unsafe extern "C" fn(u32, u32, i64, *mut i64),
    pub owner: unsafe extern "C" fn(u32) -> *mut Finances,
    pub service: unsafe extern "C" fn(*const Action) -> i64,
}
static mut ECONOMY: Economy = unsafe { std::mem::zeroed() };
static mut PRICES: [i64; 71] = [0; 71];
static mut SCORES: [[i64; 10]; 15] = [[0; 10]; 15];
static mut MULTIPLIERS: [i8; 71] = [0; 71];
static mut TICK: u32 = 0;
// SAFETY: The caller resolves a live canonical allocation on the serial game
// thread. Field types/layout are native-ABI checked. Each read/write finishes
// before a shared service or yielded action; no reference is created or retained.
macro_rules! get { ($p:expr,$($f:tt)+)=>{unsafe { addr_of!((*$p).$($f)+).read() }}; }
macro_rules! put { ($p:expr,$v:expr,$($f:tt)+)=>{{let value=$v;unsafe { addr_of_mut!((*$p).$($f)+).write(value) }}}; }
// Stable Rust allocations also carry C++ view lifetimes. Access is raw and serial.
/// Allocate zeroed private finance storage; C++ immediately starts its field objects.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_company_state_create() -> *mut Finances {
    Box::into_raw(Box::new(unsafe { std::mem::zeroed() }))
}
/// Release the sole allocation after its C++ field objects and all accesses end.
/// # Safety
/// Pointer is a live create result, owned exclusively by the caller, destroyed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_company_state_destroy(p: *mut Finances) {
    unsafe { drop(Box::from_raw(p)) };
}
/// Stable process-lifetime economy storage; C++ constructs its scalar view once.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_economy_state() -> *mut Economy {
    &raw mut ECONOMY
}
/// Stable process-lifetime prices; no concurrent accesses are permitted.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_economy_prices() -> *mut i64 {
    (&raw mut PRICES).cast()
}
/// Stable score matrix with original typed-company indexing.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_company_scores() -> *mut i64 {
    (&raw mut SCORES).cast()
}
/// Stable legacy/DATE tick-cursor address.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_company_tick() -> *mut u32 {
    &raw mut TICK
}
struct Channel {
    action: Cell<Action>,
    response: Cell<i64>,
}
struct Request {
    channel: Rc<Channel>,
    action: Option<Action>,
}
impl Future for Request {
    type Output = i64;
    fn poll(mut self: Pin<&mut Self>, _: &mut Context<'_>) -> Poll<i64> {
        if let Some(a) = self.action.take() {
            self.channel.action.set(a);
            Poll::Pending
        } else {
            Poll::Ready(self.channel.response.get())
        }
    }
}
#[derive(Clone)]
struct World {
    leaves: Leaves,
    channel: Rc<Channel>,
}
impl World {
    fn read(&self, k: u32, id: u32, a: i64) -> [i64; 32] {
        let mut out = [0; 32];
        unsafe { (self.leaves.read)(k, id, a, out.as_mut_ptr()) };
        out
    }
    fn owner(&self, id: u32) -> *mut Finances {
        unsafe { (self.leaves.owner)(id) }
    }
    fn call(&self, k: Service, id: u32, a: i64, b: i64, c: i64, d: i64) -> i64 {
        unsafe {
            (self.leaves.service)(&Action {
                kind: k as u32,
                id,
                a,
                b,
                c,
                d,
            })
        }
    }
    async fn action(&self, k: Reentry, id: u32, a: i64, b: i64, c: i64, d: i64) -> i64 {
        Request {
            channel: self.channel.clone(),
            action: Some(Action {
                kind: k as u32,
                id,
                a,
                b,
                c,
                d,
            }),
        }
        .await
    }
}
pub struct Run {
    channel: Rc<Channel>,
    future: Pin<Box<dyn Future<Output = Action>>>,
}
/// Create a continuation over serial, live synchronous shared services.
/// # Safety
/// Table entries are valid and remain callable until destruction. Read/next/owner
/// and direct services cannot throw/reenter. Named returned actions may reenter;
/// no state/world reference survives them. Source pool/ID preconditions hold.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_company_create(
    op: u32,
    id: u32,
    a: i64,
    b: i64,
    c: i64,
    d: i64,
    leaves: *const Leaves,
) -> *mut Run {
    let channel = Rc::new(Channel {
        action: Cell::new(Action::default()),
        response: Cell::new(0),
    });
    let w = World {
        leaves: unsafe { *leaves },
        channel: channel.clone(),
    };
    Box::into_raw(Box::new(Run {
        channel,
        future: Box::pin(run(w, op, id, a, b, c, d)),
    }))
}
/// Advance until an ordinary reentry action or completion; panics abort.
/// # Safety
/// Live exclusive continuation with the creation service contract; actions complete
/// on C++'s stack before resuming, with the response supplied by value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_company_advance(p: *mut Run, response: i64) -> Action {
    let r = unsafe { &mut *p };
    r.channel.response.set(response);
    match r
        .future
        .as_mut()
        .poll(&mut Context::from_waker(Waker::noop()))
    {
        Poll::Pending => r.channel.action.get(),
        Poll::Ready(a) => a,
    }
}
/// Drop a live continuation, including exceptional C++ action exits.
/// # Safety
/// Sole live creation result; no active poll or borrowed continuation storage.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_company_destroy(p: *mut Run) {
    unsafe { drop(Box::from_raw(p)) };
}
#[repr(u32)]
#[derive(Clone, Copy)]
enum Service {
    SharedRandom = 1,
    SetCurrentCompany = 2,
    InvalidateCompanyWindows = 3,
    PerformanceDirty = 4,
    UpdateHeadquarters = 5,
    AdminUpdate = 6,
    TroubleNews = 7,
    FinancialGraphsDirty = 8,
    RailMaintenance = 9,
    SignalMaintenance = 10,
    RoadMaintenance = 11,
    CanalMaintenance = 12,
    StationMaintenance = 13,
    AirportMaintenance = 14,
    RecessionNews = 15,
    PriceWindowsDirty = 16,
    SetCargoPayment = 17,
    IndustryStartup = 18,
    ClearMonitors = 19,
    ResetCompetitorTimer = 20,
    AbortCompetitorTimer = 21,
    CompanyIdentity = 22,
    MergerNews = 23,
    AcquisitionWindows = 24,
    BankruptEvents = 25,
    CompanyControlWindows = 26,
    NetworkCompanyNew = 27,
    CloseNetworkProgress = 28,
    NetworkCreationFailed = 29,
    NetworkClientCreated = 30,
    CountGroupVehicle = 31,
    ClearReplacementRules = 32,
    TransferGroup = 33,
    CopyServiceDefaults = 34,
    IntervalForVehicle = 35,
    TransferVehicleOwner = 36,
    AssignUnitNumber = 37,
    UpdateAutoreplace = 38,
    AddSignalTrack = 39,
    UpdateCrossing = 40,
    FlushSignals = 41,
    TransferAirportCount = 42,
    StationOwner = 43,
    TownRating = 44,
    TownExclusivity = 45,
    SubsidyOwner = 46,
    WaypointSignOwner = 47,
    TransferWindowOwner = 48,
    ScreenDirty = 49,
    SetLocalCompany = 50,
    ClientsToSpectators = 51,
    GiveMoneyMessage = 52,
    MoneyAnimation = 53,
    FinancesDirty = 54,
    ShowFinances = 55,
    NewYearSound = 56,
    InteractiveCompany = 57,
    ShowTakeoverDialog = 58,
    AskMergerEvent = 59,
    ScriptRandomNext = 60,
    IntervalUsesPercent = 61,
    RebuildSubsidyCache = 62,
    BankruptNews = 63,
    NewCompanyEvents = 64,
    FluctuatingEconomy = 65,
    AssertNewAISlot = 66,
}
#[repr(u32)]
#[derive(Clone, Copy)]
enum Reentry {
    PostCompanyControl = 1000,
    StartAI = 1001,
    StopAI = 1002,
    DeleteCompany = 1003,
    ChangeTileOwner = 1004,
    DeletePoolObject = 1005,
    ChangeServiceInterval = 1007,
    AllocateCompany = 1008,
}
const INVALID: u32 = u32::MAX;
const INVALID_OWNER: u32 = 255;
const COMPANIES: u32 = 15;
const PRICE_COUNT: usize = 71;
const INITIAL_LOAN: i64 = 100_000;
const LOAN_INTERVAL: i64 = 10_000;
const MAX_INFLATION: u64 = (1_u64 << 31) - 1;
const SCORE_NEEDED: [i64; 9] = [
    120, 80, 10_000, 50_000, 100_000, 40_000, 8, 10_000_000, 250_000,
];
const SCORE_WEIGHT: [i64; 9] = [100, 100, 100, 50, 100, 400, 50, 50, 50];
struct Slots {
    leaves: Leaves,
    kind: u32,
    from: u32,
}
impl Iterator for Slots {
    type Item = u32;
    fn next(&mut self) -> Option<u32> {
        let id = unsafe { (self.leaves.next)(self.kind, self.from) };
        if id == INVALID {
            None
        } else {
            self.from = id.wrapping_add(1);
            Some(id)
        }
    }
}
impl World {
    fn ids(&self, kind: u32) -> Slots {
        Slots {
            leaves: self.leaves,
            kind,
            from: 0,
        }
    }
}
fn e() -> *mut Economy {
    &raw mut ECONOMY
}
fn neg(a: i64) -> i64 {
    if a == i64::MIN { i64::MAX } else { -a }
}
fn result(a: i64, b: i64, c: i64, d: i64) -> Action {
    Action {
        a,
        b,
        c,
        d,
        ..Action::default()
    }
}
fn maxloan(p: *mut Finances) -> i64 {
    let max = get!(p, max_loan);
    if max == i64::MIN {
        get!(e(), max_loan)
    } else {
        max
    }
}
fn available(w: &World, id: u32) -> i64 {
    if w.read(0, 0, 0)[6] != 0 || w.owner(id).is_null() {
        i64::MAX
    } else {
        get!(w.owner(id), money)
    }
}
fn subtract(w: &World, id: u32, cost: i64, expense: usize) {
    if cost == 0 {
        return;
    }
    let p = w.owner(id);
    put!(p, get!(p, money).saturating_sub(cost), money);
    put!(
        p,
        get!(p, expenses[0][expense]).saturating_add(cost),
        expenses[0][expense]
    );
    if (7..=10).contains(&expense) {
        put!(
            p,
            get!(p, current.income).saturating_sub(cost),
            current.income
        );
    } else if (2..=6).contains(&expense) || expense == 11 {
        put!(
            p,
            get!(p, current.expenses).saturating_sub(cost),
            current.expenses
        );
    }
    w.call(Service::InvalidateCompanyWindows, id, 0, 0, 0, 0);
}
fn subtract_fraction(w: &World, id: u32, mut cost: i64, expense: usize) {
    let p = w.owner(id);
    let old = get!(p, fraction);
    let next = old.wrapping_sub(cost as u8);
    put!(p, next, fraction);
    cost >>= 8;
    if next > old {
        cost = cost.saturating_add(1);
    }
    if cost != 0 {
        subtract(w, id, cost, expense);
    }
}
fn assets(w: &World, id: u32) -> i64 {
    let mut num = 0_u32;
    for st in w.ids(3) {
        let s = w.read(3, st, 0);
        if s[0] as u32 == id {
            num = num.wrapping_add(s[1] as u32);
        }
    }
    let price = unsafe { (&raw const PRICES).cast::<i64>().read() };
    let mut value = price.saturating_mul(i64::from(num)).saturating_mul(25);
    for v in w.ids(2) {
        let x = w.read(2, v, 0);
        if x[0] as u32 != id {
            continue;
        }
        if x[1] == 0 || x[1] == 1 || (x[1] == 3 && x[5] != 0) || x[1] == 2 {
            value = value.saturating_add(x[6].saturating_mul(3) >> 1);
        }
    }
    value
}
fn value(w: &World, id: u32, including_loan: bool) -> i64 {
    let mut amount = assets(w, id);
    let p = w.owner(id);
    if including_loan {
        amount = amount.saturating_sub(get!(p, loan));
    }
    amount.saturating_add(get!(p, money)).max(1)
}
fn hostile_value(w: &World, id: u32) -> i64 {
    let mut amount = assets(w, id);
    let p = w.owner(id);
    amount = amount.saturating_add(get!(p, loan));
    if get!(p, money) < 0 {
        amount = amount.saturating_add(neg(get!(p, money)));
    }
    for quarter in 0..4 {
        amount = amount.saturating_add(
            get!(p, old[quarter].income)
                .saturating_add(get!(p, old[quarter].expenses))
                .max(0)
                .saturating_mul(2),
        );
    }
    amount.max(1)
}
fn rating(w: &World, id: u32, update: bool) -> i32 {
    let mut part = [0_i64; 10];
    let p = w.owner(id);
    let min_age = w.read(0, 0, 0)[26];
    let mut min_profit = 0;
    let mut first = true;
    let mut count = 0_u32;
    for idv in w.ids(2) {
        let v = w.read(2, idv, 0);
        if v[0] as u32 != id {
            continue;
        }
        if v[1] < 4 && v[2] != 0 {
            if v[7] > 0 {
                count = count.wrapping_add(1);
            }
            if v[8] > min_age && (first || min_profit > v[7]) {
                min_profit = v[7];
                first = false;
            }
        }
    }
    part[0] = i64::from(count);
    part[2] = (min_profit >> 8).max(0);
    let mut count = 0_u32;
    for sid in w.ids(3) {
        let s = w.read(3, sid, 0);
        if s[0] as u32 == id && (s[2] <= 20 || s[3] <= 20) {
            count = count.wrapping_add(s[1] as u32);
        }
    }
    part[1] = i64::from(count);
    let entries = usize::from(get!(p, valid)).min(12);
    if entries != 0 {
        let mut min = i64::MAX;
        let mut max = i64::MIN;
        for i in 0..entries {
            let income = get!(p, old[i].income).saturating_add(get!(p, old[i].expenses));
            min = min.min(income);
            max = max.max(income);
        }
        part[3] = min.max(0);
        part[4] = max;
    }
    for i in 0..usize::from(get!(p, valid)).min(4) {
        for c in 0..64 {
            part[5] = part[5].saturating_add(i64::from(get!(p, old[i].cargo[c])));
        }
    }
    for c in 0..64 {
        part[6] += i64::from(get!(p, old[0].cargo[c]) != 0);
    }
    part[7] = get!(p, money).max(0);
    part[8] = 250_000_i64.saturating_sub(get!(p, loan));
    let mut score = 0;
    let mut total = 0;
    for i in 0..9 {
        score += part[i].clamp(0, SCORE_NEEDED[i]) * SCORE_WEIGHT[i] / SCORE_NEEDED[i];
        total += SCORE_WEIGHT[i];
    }
    part[9] = score;
    unsafe {
        addr_of_mut!(SCORES)
            .cast::<[i64; 10]>()
            .add(id as usize)
            .write(part);
    }
    if total != 1000 {
        score = score * 1000 / total;
    }
    if update {
        put!(p, score as i32, old[0].performance);
        let hq = w.read(1, id, 0)[3];
        w.call(Service::UpdateHeadquarters, id, hq, score, 0, 0);
        let v = value(w, id, true);
        put!(p, v, old[0].value);
    }
    w.call(Service::PerformanceDirty, id, 0, 0, 0, 0);
    score as i32
}
fn takeover_limit(w: &World, big: u32, small: u32) -> bool {
    let a = w.read(1, big, 0);
    let b = w.read(1, small, 0);
    let limits = w.read(0, 0, 2);
    for i in 0..4 {
        if (a[4 + i] as u32).wrapping_add(b[4 + i] as u32) > limits[i] as u32 {
            return false;
        }
    }
    true
}
async fn bankruptcy(w: &World, id: u32) {
    if w.read(0, 0, 0)[6] != 0 {
        return;
    }
    let p = w.owner(id);
    if get!(p, money).saturating_sub(get!(p, loan)) >= neg(maxloan(p)) {
        let previous = u32::from(get!(p, bankruptcy)).div_ceil(3);
        put!(p, 0, bankruptcy);
        put!(p, 0, asked);
        if previous != 0 {
            w.call(Service::AdminUpdate, id, 0, 0, 0, 0);
        }
        return;
    }
    let month = get!(p, bankruptcy).wrapping_add(1);
    put!(p, month, bankruptcy);
    match month {
        0..=3 | 5 | 6 | 8 | 9 => {}
        4 => {
            w.call(Service::TroubleNews, id, 0, 0, 0, 0);
        }
        7 => {
            let v = value(w, id, false);
            put!(p, v, bankrupt_value);
            put!(p, 1_u16 << id, asked);
            put!(p, 0, timeout);
        }
        _ => {
            let world = w.read(0, 0, 0);
            if world[2] == 0 && world[4] as u32 == id {
                put!(p, u16::MAX, asked);
            } else if world[2] == 0 || world[3] != 0 {
                w.action(Reentry::PostCompanyControl, id, 2, 2, u32::MAX.into(), 0)
                    .await;
                return;
            }
        }
    }
    // Original subtraction promotes uint8_t to int before ceil division.
    if (i32::from(month) + 2) / 3 != (i32::from(month) - 1 + 2) / 3 {
        w.call(Service::AdminUpdate, id, 0, 0, 0, 0);
    }
}
async fn statistics(w: &World) {
    for id in w.ids(1) {
        bankruptcy(w, id).await;
    }
    let current = w.read(0, 0, 0)[5];
    if w.read(0, 0, 0)[10] != 0 {
        for id in w.ids(1) {
            w.call(Service::SetCurrentCompany, id, 0, 0, 0, 0);
            let infra = w.read(12, id, -1);
            let mut cost = 0_i64;
            for rt in 0..w.read(0, 0, 0)[23] as u32 {
                let amount = w.read(12, id, i64::from(rt))[0];
                if amount != 0 {
                    cost = cost.saturating_add(w.call(
                        Service::RailMaintenance,
                        rt,
                        amount,
                        infra[2],
                        0,
                        0,
                    ));
                }
            }
            cost = cost.saturating_add(w.call(Service::SignalMaintenance, id, infra[5], 0, 0, 0));
            for rt in 0..w.read(0, 0, 0)[24] as u32 {
                let r = w.read(12, id, 100 + i64::from(rt));
                if r[0] != 0 {
                    cost = cost.saturating_add(w.call(
                        Service::RoadMaintenance,
                        rt,
                        r[0],
                        if r[1] != 0 { infra[3] } else { infra[4] },
                        0,
                        0,
                    ));
                }
            }
            cost = cost.saturating_add(w.call(Service::CanalMaintenance, id, infra[6], 0, 0, 0));
            cost = cost.saturating_add(w.call(Service::StationMaintenance, id, infra[7], 0, 0, 0));
            cost = cost.saturating_add(w.call(Service::AirportMaintenance, id, 0, 0, 0, 0));
            subtract(w, id, cost, 6);
        }
    }
    w.call(Service::SetCurrentCompany, current as u32, 0, 0, 0, 0);
    if ![0, 3, 6, 9].contains(&w.read(0, 0, 0)[7]) {
        return;
    }
    for id in w.ids(1) {
        let p = w.owner(id);
        for i in (1..24).rev() {
            put!(p, get!(p, old[i - 1]), old[i]);
        }
        put!(p, get!(p, current), old[0]);
        put!(p, unsafe { std::mem::zeroed() }, current);
        if get!(p, valid) != 24 {
            put!(p, get!(p, valid).wrapping_add(1), valid);
        }
        rating(w, id, true);
        if get!(p, preview) != 0 {
            put!(p, get!(p, preview).wrapping_sub(1), preview);
        }
    }
    w.call(Service::FinancialGraphsDirty, 0, 0, 0, 0, 0);
}
fn inflation(w: &World, check: bool) -> bool {
    let world = w.read(0, 0, 0);
    if check && (world[8] < world[21] || world[8] >= world[22]) {
        return true;
    }
    let prices = get!(e(), prices);
    let payment = get!(e(), payment);
    if prices == MAX_INFLATION || payment == MAX_INFLATION {
        return true;
    }
    put!(
        e(),
        prices
            .wrapping_add(
                prices
                    .wrapping_mul(u64::from(get!(e(), infl_amount)))
                    .wrapping_mul(54)
                    >> 16
            )
            .min(MAX_INFLATION),
        prices
    );
    put!(
        e(),
        payment
            .wrapping_add(
                payment
                    .wrapping_mul(u64::from(get!(e(), infl_payment)))
                    .wrapping_mul(54)
                    >> 16
            )
            .min(MAX_INFLATION),
        payment
    );
    false
}
fn recompute(w: &World) {
    let world = w.read(0, 0, 0);
    put!(
        e(),
        (((world[27] as u64).wrapping_mul(get!(e(), prices)) >> 16) / 10_000 * 10_000) as i64,
        max_loan
    );
    for i in 0..PRICE_COUNT {
        let spec = w.read(13, i as u32, 0);
        let modif = match spec[1] {
            1 => world[28],
            2 => world[29],
            _ => 1,
        };
        let mut price = spec[0].saturating_mul(match modif {
            0 => 6,
            1 => 8,
            2 => 9,
            _ => unreachable!(),
        });
        price = (price as u64).wrapping_mul(get!(e(), prices)) as i64;
        let shift = i32::from(unsafe { addr_of!(MULTIPLIERS).cast::<i8>().add(i).read() }) - 19;
        price = if shift >= 0 {
            price.wrapping_shl(shift as u32)
        } else {
            price >> (-shift)
        };
        if price == 0 {
            price = spec[0].clamp(-1, 1);
        }
        unsafe {
            addr_of_mut!(PRICES).cast::<i64>().add(i).write(price);
        }
    }
    for id in w.ids(14) {
        let p = w.read(14, id, 0)[0];
        w.call(
            Service::SetCargoPayment,
            id,
            p.saturating_mul(get!(e(), payment) as i64) >> 16,
            0,
            0,
            0,
        );
    }
    w.call(Service::PriceWindowsDirty, 0, 0, 0, 0, 0);
}
fn interest(w: &World) {
    let current = w.read(0, 0, 0)[5];
    for id in w.ids(1) {
        w.call(Service::SetCurrentCompany, id, 0, 0, 0, 0);
        let p = w.owner(id);
        let rate = i64::from(get!(e(), interest));
        let mut fee = get!(p, loan).saturating_mul(rate) / 100;
        let cash = available(w, id);
        if cash < 0 {
            fee = fee.saturating_add(neg(cash).saturating_mul(rate) / 100);
        }
        let month = w.read(0, 0, 0)[7];
        let previous = fee.saturating_mul(month) / 12;
        let now = fee.saturating_mul(month + 1) / 12;
        subtract(w, id, now.saturating_sub(previous), 11);
        subtract(
            w,
            id,
            unsafe { addr_of!(PRICES).cast::<i64>().read() } >> 2,
            12,
        );
    }
    w.call(Service::SetCurrentCompany, current as u32, 0, 0, 0, 0);
}
fn fluctuations(w: &World) {
    if w.call(Service::FluctuatingEconomy, 0, 0, 0, 0, 0) != 0 {
        put!(e(), get!(e(), fluct).wrapping_sub(1), fluct);
    } else if get!(e(), fluct) <= 0 {
        put!(e(), -12, fluct);
    } else {
        return;
    }
    if get!(e(), fluct) == 0 {
        put!(
            e(),
            -((w.call(Service::SharedRandom, 0, 0, 0, 0, 0) as u32 & 3) as i16),
            fluct
        );
        w.call(Service::RecessionNews, 0, 1, 0, 0, 0);
    } else if get!(e(), fluct) == -12 {
        put!(
            e(),
            ((w.call(Service::SharedRandom, 0, 0, 0, 0, 0) as u32 & 255) + 312) as i16,
            fluct
        );
        w.call(Service::RecessionNews, 0, 0, 0, 0, 0);
    }
}
fn offer(w: &World, id: u32) {
    let p = w.owner(id);
    let timeout = get!(p, timeout);
    if timeout != 0 {
        let next = timeout.wrapping_sub(COMPANIES as i16);
        put!(p, next, timeout);
        if next <= 0 {
            put!(p, 0, timeout);
        }
        return;
    }
    if get!(p, asked) == u16::MAX {
        return;
    }
    let mut best = INVALID;
    let mut performance = -1;
    for other in w.ids(1) {
        let q = w.owner(other);
        let score = get!(q, old[1].performance);
        if get!(q, asked) == 0
            && get!(p, asked) & (1 << other) == 0
            && performance < score
            && takeover_limit(w, other, id)
        {
            performance = score;
            best = other;
        }
    }
    if performance == -1 {
        put!(p, u16::MAX, asked);
        return;
    }
    put!(p, get!(p, asked) | (1 << best), asked);
    put!(
        p,
        (3 * 30 * w.read(0, 0, 0)[18] / i64::from(COMPANIES - 1)) as i16,
        timeout
    );
    w.call(
        Service::AskMergerEvent,
        best,
        i64::from(id),
        get!(p, bankrupt_value),
        0,
        0,
    );
    if w.call(Service::InteractiveCompany, best, 0, 0, 0, 0) != 0 {
        w.call(Service::ShowTakeoverDialog, id, 0, 0, 0, 0);
    }
}
async fn competitor_timeout(w: &World) {
    let world = w.read(0, 0, 0);
    if world[1] != 0
        || world[16] == 0
        || (world[2] != 0 && world[15] >= world[14])
        || world[12] == 0
    {
        return;
    }
    let mut n = 0_u8;
    for id in w.ids(1) {
        if w.read(1, id, 0)[1] != 0 {
            n = n.wrapping_add(1);
        }
    }
    if i64::from(n) >= world[13] {
        return;
    }
    w.action(Reentry::PostCompanyControl, 255, 1, 0, u32::MAX.into(), 0)
        .await;
}
async fn tick(w: &World) {
    if w.read(0, 0, 0)[0] != 0 {
        return;
    }
    let id = unsafe { TICK };
    if !w.owner(id).is_null() {
        if w.read(1, id, 0)[2] != 0 {
            w.call(Service::CompanyIdentity, id, 0, 0, 0, 0);
        }
        if get!(w.owner(id), asked) != 0 {
            offer(w, id);
        }
    }
    let world = w.read(0, 0, 0);
    if world[17] != 0 && world[1] == 0 && world[16] != 0 {
        let mut timeout = (world[12] as i32)
            .wrapping_mul(60)
            .wrapping_mul(world[19] as i32);
        if timeout == 0 {
            let mut n = 0_u8;
            for id in w.ids(1) {
                if w.read(1, id, 0)[1] != 0 {
                    n = n.wrapping_add(1);
                }
            }
            let mut count = world[15] as usize;
            for _ in 0..world[13] {
                if world[2] != 0 {
                    let old = count;
                    count = count.wrapping_add(1);
                    if old >= world[14] as usize {
                        break;
                    }
                }
                let old = n;
                n = n.wrapping_add(1);
                if i64::from(old) >= world[13] {
                    break;
                }
                w.action(Reentry::PostCompanyControl, 255, 1, 0, u32::MAX.into(), 0)
                    .await;
            }
            timeout = 10 * 60 * world[19] as i32;
        }
        timeout = timeout
            .wrapping_add(w.call(
                Service::ScriptRandomNext,
                0,
                i64::from(timeout / 4),
                0,
                0,
                0,
            ) as i32)
            .wrapping_sub(timeout / 8);
        w.call(
            Service::ResetCompetitorTimer,
            0,
            i64::from(timeout.max(1)),
            0,
            0,
            0,
        );
    }
    unsafe {
        TICK = TICK.wrapping_add(1) % COMPANIES;
    }
}
fn yearly(w: &World) {
    for id in w.ids(1) {
        let p = w.owner(id);
        put!(p, get!(p, expenses[1]), expenses[2]);
        put!(p, get!(p, expenses[0]), expenses[1]);
        put!(p, [0; 13], expenses[0]);
        w.call(Service::FinancesDirty, id, 0, 0, 0, 0);
    }
    let world = w.read(0, 0, 0);
    let ui = w.read(0, 0, 3);
    if ui[0] != 0 && world[4] != 255 {
        let id = world[4] as u32;
        w.call(Service::ShowFinances, id, 0, 0, 0, 0);
        let p = w.owner(id);
        let bad = get!(p, valid) > 5 && get!(p, old[0].performance) < get!(p, old[4].performance);
        if ui[1] != 0 {
            w.call(Service::NewYearSound, id, i64::from(bad), 0, 0, 0);
        }
    }
}
async fn startup_company(w: &World, is_ai: bool, requested: u32) -> u32 {
    if w.read(0, 0, 0)[31] == 0 {
        return INVALID;
    }
    let colour = w.call(Service::CompanyIdentity, 0, 1, 0, 0, 0);
    if requested != 255 && !w.owner(requested).is_null() {
        return INVALID;
    }
    let id = w
        .action(
            Reentry::AllocateCompany,
            requested,
            i64::from(is_ai),
            0,
            0,
            0,
        )
        .await as u32;
    w.call(Service::CompanyIdentity, id, 2, colour, 0, 0);
    let loan =
        (((INITIAL_LOAN as u64).wrapping_mul(get!(e(), prices)) >> 16) / 10_000 * 10_000) as i64;
    let p = w.owner(id);
    let loan = loan.min(get!(e(), max_loan));
    put!(p, loan, loan);
    put!(p, loan, money);
    w.call(Service::CompanyIdentity, id, 3, 0, 0, 0);
    w.call(Service::CompanyIdentity, id, 4, i64::from(is_ai), 0, 0);
    w.call(Service::CompanyIdentity, id, 5, 0, 0, 0);
    w.call(Service::CompanyIdentity, id, 6, 0, 0, 0);
    let world = w.read(0, 0, 0);
    if is_ai && (world[2] == 0 || world[3] != 0) {
        w.action(Reentry::StartAI, id, 0, 0, 0, 0).await;
    }
    w.call(Service::NewCompanyEvents, id, 0, 0, 0, 0);
    id
}
async fn transfer(w: &World, old: u32, new: u32) {
    let current = w.read(0, 0, 0)[5];
    w.call(Service::SetCurrentCompany, old, 0, 0, 0, 0);
    if w.read(0, 0, 0)[2] != 0 {
        w.call(Service::ClientsToSpectators, old, 0, 0, 0, 0);
    }
    if w.read(0, 0, 0)[4] as u32 == old {
        let saved = w.read(0, 0, 0)[5];
        for id in w.ids(1) {
            if id != old {
                w.call(Service::SetLocalCompany, id, 0, 0, 0, 0);
                break;
            }
        }
        w.call(Service::SetCurrentCompany, saved as u32, 0, 0, 0, 0);
    }
    if new == INVALID_OWNER {
        put!(w.owner(old), (u64::MAX >> 2) as i64, money);
    }
    for id in w.ids(4) {
        if w.read(4, id, 0)[0] as u32 == old {
            if new == INVALID_OWNER {
                w.action(Reentry::DeletePoolObject, id, 4, 0, 0, 0).await;
            } else {
                w.call(Service::SubsidyOwner, id, i64::from(new), 0, 0, 0);
            }
        }
    }
    if new == INVALID_OWNER {
        w.call(Service::RebuildSubsidyCache, 0, 0, 0, 0, 0);
    }
    for id in w.ids(5) {
        let t = w.read(5, id, i64::from(old));
        if new != INVALID_OWNER && t[1] as u16 & (1 << old) != 0 {
            let n = w.read(5, id, i64::from(new));
            let rating = if n[1] as u16 & (1 << new) != 0 {
                n[0].max(t[0])
            } else {
                t[0]
            };
            w.call(Service::TownRating, id, i64::from(new), rating, 1, 0);
        }
        w.call(Service::TownRating, id, i64::from(old), 500, 0, 0);
        if t[2] > 0 && t[3] as u32 == old {
            w.call(
                Service::TownExclusivity,
                id,
                i64::from(new),
                if new == INVALID_OWNER { 0 } else { t[2] },
                0,
                0,
            );
        }
    }
    for id in w.ids(2) {
        let v = w.read(2, id, 0);
        if v[0] as u32 == old && v[1] < 4 {
            if new == INVALID_OWNER {
                if v[4] != 0 {
                    w.action(Reentry::DeletePoolObject, id, 2, 0, 0, 0).await;
                }
            } else {
                if v[3] != 0 {
                    w.call(Service::CountGroupVehicle, id, -1, 0, 0, 0);
                }
                if v[2] != 0 {
                    w.call(Service::CountGroupVehicle, id, -1, 1, 0, 0);
                }
            }
        }
    }
    w.call(Service::ClearReplacementRules, old, 0, 0, 0, 0);
    for id in w.ids(6) {
        if w.read(6, id, 0)[0] as u32 == old {
            if new == INVALID_OWNER {
                w.action(Reentry::DeletePoolObject, id, 6, 0, 0, 0).await;
            } else {
                w.call(Service::TransferGroup, id, i64::from(new), 0, 0, 0);
            }
        }
    }
    if new != INVALID_OWNER {
        w.call(Service::CopyServiceDefaults, old, i64::from(new), 0, 0, 0);
    }
    for id in w.ids(2) {
        let v = w.read(2, id, 0);
        if v[0] as u32 == old && v[1] < 4 {
            if v[9] == 0 {
                let interval = w.call(Service::IntervalForVehicle, new, v[1], 0, 0, 0);
                let percent = w.call(Service::IntervalUsesPercent, new, 0, 0, 0, 0);
                w.action(Reentry::ChangeServiceInterval, id, interval, percent, 0, 0)
                    .await;
            }
            w.call(Service::TransferVehicleOwner, id, i64::from(new), 0, 0, 0);
            let v = w.read(2, id, 0);
            if v[3] != 0 {
                w.call(Service::CountGroupVehicle, id, 1, 0, 0, 0);
            }
            if v[2] != 0 {
                w.call(Service::CountGroupVehicle, id, 1, 1, 0, 0);
                w.call(Service::AssignUnitNumber, id, 0, 0, 0, 0);
            }
        }
    }
    if new != INVALID_OWNER {
        w.call(Service::UpdateAutoreplace, new, 0, 0, 0, 0);
    }
    let map_size = w.read(11, 0, -1)[0] as u32;
    for tile in 0..map_size {
        w.action(
            Reentry::ChangeTileOwner,
            tile,
            i64::from(old),
            i64::from(new),
            0,
            0,
        )
        .await;
    }
    if new != INVALID_OWNER {
        for tile in 0..map_size {
            let t = w.read(11, tile, -1);
            if t[1] != 0 && t[2] as u32 == new && t[3] != 0 {
                let mut tracks = t[5] as u32;
                loop {
                    let track = tracks.trailing_zeros();
                    tracks &= tracks.wrapping_sub(1);
                    if w.read(11, tile, i64::from(track))[6] != 0 {
                        w.call(
                            Service::AddSignalTrack,
                            tile,
                            i64::from(track),
                            i64::from(new),
                            0,
                            0,
                        );
                    }
                    if tracks == 0 {
                        break;
                    }
                }
            } else if t[4] != 0 && t[2] as u32 == new {
                w.call(Service::UpdateCrossing, tile, 0, 0, 0, 0);
            }
        }
    }
    w.call(Service::FlushSignals, 0, 0, 0, 0, 0);
    if new != INVALID_OWNER {
        w.call(Service::TransferAirportCount, new, i64::from(old), 0, 0, 0);
    }
    let owner = if new == INVALID_OWNER { 16 } else { new };
    for id in w.ids(3) {
        if w.read(3, id, 0)[0] as u32 == old {
            w.call(Service::StationOwner, id, i64::from(owner), 0, 0, 0);
        }
    }
    for kind in [7, 8] {
        for id in w.ids(kind) {
            if w.read(kind, id, 0)[0] as u32 == old {
                w.call(
                    Service::WaypointSignOwner,
                    id,
                    i64::from(owner),
                    i64::from(kind),
                    0,
                    0,
                );
            }
        }
    }
    for id in w.ids(9) {
        if w.read(9, id, 0)[0] as u32 == old {
            w.action(Reentry::DeletePoolObject, id, 9, 0, 0, 0).await;
        }
    }
    w.call(Service::ClearMonitors, old, 1, 0, 0, 0);
    for id in w.ids(10) {
        if w.read(10, id, 0)[0] as u32 == old {
            w.action(Reentry::DeletePoolObject, id, 10, 0, 0, 0).await;
        }
    }
    if new != INVALID_OWNER {
        w.call(Service::TransferWindowOwner, old, i64::from(new), 0, 0, 0);
    }
    w.call(Service::SetCurrentCompany, current as u32, 0, 0, 0, 0);
    w.call(Service::ScreenDirty, 0, 0, 0, 0, 0);
}
async fn company_ctrl(
    w: &World,
    target: u32,
    action: i64,
    execute: bool,
    reason: i64,
    client: i64,
) -> Action {
    w.call(Service::CompanyControlWindows, 0, 0, 0, 0, 0);
    match action {
        0 => {
            if w.read(0, 0, 0)[2] == 0 {
                return result(1, 0, 255, 0);
            }
            if !execute {
                return result(0, 0, 255, 0);
            }
            // Client identity is preserved by the native networking services.
            w.call(Service::CloseNetworkProgress, 0, 0, 0, 0, 0);
            let id = startup_company(w, false, 255).await;
            if id == INVALID {
                w.call(Service::NetworkCreationFailed, 0, client, 0, 0, 0);
            } else {
                w.call(Service::NetworkCompanyNew, id, client, 0, 0, 0);
                w.call(Service::NetworkClientCreated, id, client, 0, 0, 0);
            }
        }
        1 => {
            if target != 255 && target >= COMPANIES {
                return result(1, 0, 255, 0);
            }
            if w.read(0, 0, 0)[2] == 0 && target != 255 && !w.owner(target).is_null() {
                return result(1, 0, 255, 0);
            }
            if !execute {
                return result(0, 0, 255, 0);
            }
            w.call(Service::AssertNewAISlot, target, 0, 0, 0, 0);
            let id = startup_company(w, true, target).await;
            if id != INVALID {
                w.call(Service::NetworkCompanyNew, id, -1, 0, 0, 0);
            }
        }
        2 => {
            if reason >= 3 {
                return result(1, 0, 255, 0);
            }
            let world = w.read(0, 0, 0);
            if world[2] == 0 && world[15] == 1 {
                return result(1, 0, 255, 0);
            }
            if w.owner(target).is_null() {
                return result(1, 0, 255, 0);
            }
            if !execute {
                return result(0, 0, 255, 0);
            }
            w.call(Service::BankruptNews, target, 0, 0, 0, 0);
            transfer(w, target, 255).await;
            if w.read(1, target, 0)[1] != 0 {
                w.action(Reentry::StopAI, target, 0, 0, 0, 0).await;
            }
            w.action(Reentry::DeleteCompany, target, 0, 0, 0, 0).await;
            w.call(Service::BankruptEvents, target, reason, 0, 0, 0);
        }
        _ => return result(1, 0, 255, 0),
    }
    w.call(Service::CompanyControlWindows, 0, 1, 0, 0, 0);
    result(0, 0, 255, 0)
}
async fn buy(w: &World, id: u32, mut hostile: bool, execute: bool) -> Action {
    let p = w.owner(id);
    if p.is_null() {
        return result(1, 0, 255, 0);
    }
    let current = w.read(0, 0, 0)[5] as u32;
    let asked = get!(p, asked) & (1 << current) != 0;
    if hostile && asked {
        hostile = false;
    }
    if !hostile && !asked {
        return result(1, 0, 255, 0);
    }
    if hostile && w.read(1, id, 0)[1] == 0 {
        return result(1, 0, 255, 0);
    }
    let world = w.read(0, 0, 0);
    if hostile && world[2] != 0 {
        return result(1, 0, 255, 0);
    }
    if world[2] == 0 && world[4] as u32 == id {
        return result(1, 0, 255, 0);
    }
    if id == current {
        return result(1, 0, 255, 0);
    }
    if !takeover_limit(w, current, id) {
        return result(6, 0, 255, 0);
    }
    let cost = if hostile {
        hostile_value(w, id)
    } else {
        get!(p, bankrupt_value)
    };
    if execute {
        w.call(Service::MergerNews, id, i64::from(hostile), 0, 0, 0);
        transfer(w, id, current).await;
        if w.read(1, id, 0)[1] != 0 {
            w.action(Reentry::StopAI, id, 0, 0, 0, 0).await;
        }
        w.call(Service::AcquisitionWindows, id, 0, 0, 0, 0);
        w.action(Reentry::DeleteCompany, id, 0, 0, 0, 0).await;
    }
    result(0, cost, 12, 0)
}
fn loan(w: &World, increase: bool, cmd: i64, amount: i64, execute: bool) -> Action {
    let id = w.read(0, 0, 0)[5] as u32;
    let p = w.owner(id);
    let current = get!(p, loan);
    let max = maxloan(p);
    if increase && current >= max {
        return result(2, 0, 255, max);
    }
    if !increase && current == 0 {
        return result(3, 0, 255, 0);
    }
    let mut loan = match cmd {
        0 => {
            if increase {
                LOAN_INTERVAL
            } else {
                current.min(LOAN_INTERVAL)
            }
        }
        1 => {
            if increase {
                max.saturating_sub(current)
            } else {
                current.min(available(w, id)).max(LOAN_INTERVAL)
            }
        }
        2 => amount,
        _ => return result(1, 0, 255, 0),
    };
    if cmd == 2 {
        if increase {
            if loan < LOAN_INTERVAL
                || current.saturating_add(loan) > max
                || loan % LOAN_INTERVAL != 0
            {
                return result(1, 0, 255, 0);
            }
        } else if loan % LOAN_INTERVAL != 0 || loan < LOAN_INTERVAL || loan > current {
            return result(1, 0, 255, 0);
        }
    }
    if !increase && cmd == 1 {
        loan = loan.saturating_sub(loan % LOAN_INTERVAL);
    }
    if increase && get!(p, money) > i64::MAX.saturating_sub(loan) {
        return result(1, 0, 255, 0);
    }
    if !increase && available(w, id) < loan {
        return result(4, 0, 255, loan);
    }
    if execute {
        if increase {
            put!(p, get!(p, money).saturating_add(loan), money);
            put!(p, current.saturating_add(loan), loan);
        } else {
            put!(p, get!(p, money).saturating_sub(loan), money);
            put!(p, current.saturating_sub(loan), loan);
        }
        w.call(Service::InvalidateCompanyWindows, id, 0, 0, 0, 0);
    }
    result(0, 0, if increase { 12 } else { 255 }, 0)
}
async fn run(w: World, op: u32, id: u32, a: i64, b: i64, c: i64, d: i64) -> Action {
    match op {
        0 => {
            subtract(&w, id, a, b as usize);
        }
        1 => {
            subtract_fraction(&w, id, a, b as usize);
        }
        2 => {
            for id in w.ids(1) {
                let p = w.owner(id);
                let rates = w.read(0, 0, 1);
                for i in 0..4 {
                    let field = unsafe { addr_of_mut!((*p).terraform).add(i) };
                    let value = unsafe { field.read() };
                    let next = (u64::from(value).wrapping_add(rates[i * 2] as u64))
                        .min((rates[i * 2 + 1] as u64).wrapping_shl(16));
                    unsafe {
                        field.write(next as u32);
                    }
                }
            }
        }
        3 => return result(0, assets(&w, id), 255, 0),
        4 => return result(0, value(&w, id, a != 0), 255, 0),
        5 => return result(0, hostile_value(&w, id), 255, 0),
        6 => return result(0, i64::from(rating(&w, id, a != 0)), 255, 0),
        7 => {
            statistics(&w).await;
            interest(&w);
            fluctuations(&w);
        }
        8 => return result(0, i64::from(inflation(&w, a != 0)), 255, 0),
        9 => recompute(&w),
        10 => unsafe {
            MULTIPLIERS = [0; PRICE_COUNT];
        },
        11 => unsafe {
            addr_of_mut!(MULTIPLIERS)
                .cast::<i8>()
                .add(id as usize)
                .write(a.clamp(-8, 16) as i8);
        },
        12 => {
            let world = w.read(0, 0, 0);
            put!(e(), world[30] as u8, interest);
            put!(e(), world[30] as u8, infl_amount);
            put!(e(), (world[30] - 1).max(0) as u8, infl_payment);
            put!(
                e(),
                ((w.call(Service::SharedRandom, 0, 0, 0, 0, 0) as u32 & 255) + 168) as i16,
                fluct
            );
            if world[9] != 0 {
                let months = (world[8].min(world[22]) - world[21]) as i32 * 12;
                for _ in 0..months {
                    inflation(&w, false);
                }
            }
            recompute(&w);
            w.call(Service::IndustryStartup, 0, 1, 0, 0, 0);
        }
        13 => {
            put!(e(), 1 << 16, prices);
            put!(e(), 1 << 16, payment);
            w.call(Service::ClearMonitors, 0, 0, 0, 0, 0);
        }
        14 => {
            if id >= PRICE_COUNT as u32 {
                return result(0, 0, 255, 0);
            }
            let mut price = unsafe { addr_of!(PRICES).cast::<i64>().add(id as usize).read() }
                .saturating_mul(i64::from(a as u32));
            let shift = b as i32;
            price = if shift >= 0 {
                price.wrapping_shl(shift as u32)
            } else {
                price >> (-shift)
            };
            return result(0, price, 255, 0);
        }
        15 => return result(0, i64::from(startup_company(&w, a != 0, id).await), 255, 0),
        16 => competitor_timeout(&w).await,
        17 => tick(&w).await,
        18 => yearly(&w),
        19 => return result(0, i64::from(takeover_limit(&w, id, a as u32)), 255, 0),
        20 => return company_ctrl(&w, id, a, b != 0, c, d).await,
        21 => {
            if w.read(0, 0, 0)[11] == 0 {
                return result(1, 0, 255, 0);
            }
            let current = w.read(0, 0, 0)[5] as u32;
            let p = w.owner(current);
            let amount = a.min(20_000_000);
            if get!(p, money).saturating_sub(get!(p, loan)) < amount || amount < 0 {
                return result(5, 0, 255, 0);
            }
            if w.owner(id).is_null() {
                return result(1, 0, 255, 0);
            }
            if b != 0 {
                w.call(Service::SetCurrentCompany, id, 0, 0, 0, 0);
                subtract(&w, id, neg(amount), 12);
                w.call(Service::SetCurrentCompany, current, 0, 0, 0, 0);
                if w.read(0, 0, 0)[2] != 0 {
                    w.call(Service::GiveMoneyMessage, id, amount, 0, 0, 0);
                }
            }
            return result(0, amount, 12, 0);
        }
        22 => return loan(&w, true, a, b, c != 0),
        23 => return loan(&w, false, a, b, c != 0),
        24 => {
            if w.read(0, 0, 0)[5] != 18 {
                return result(1, 0, 255, 0);
            }
            if a != i64::MIN && !(0..=2_000_000_000).contains(&a) {
                return result(1, 0, 255, 0);
            }
            let p = w.owner(id);
            if p.is_null() {
                return result(1, 0, 255, 0);
            }
            if b != 0 {
                let amount = if a == i64::MIN {
                    a
                } else {
                    a - a % LOAN_INTERVAL
                };
                put!(p, amount, max_loan);
                w.call(Service::InvalidateCompanyWindows, id, 0, 0, 0, 0);
            }
        }
        25 => {
            if w.owner(id).is_null() || b >= 13 || w.read(0, 0, 0)[5] != 18 {
                return result(1, 0, 255, 0);
            }
            if c != 0 {
                let current = w.read(0, 0, 0)[5] as u32;
                w.call(Service::SetCurrentCompany, id, 0, 0, 0, 0);
                subtract(&w, id, neg(a), b as usize);
                w.call(Service::SetCurrentCompany, current, 0, 0, 0, 0);
                if d != 0 {
                    w.call(Service::MoneyAnimation, d as u32, neg(a), 0, 0, 0);
                }
            }
            return result(0, 0, b, 0);
        }
        26 => transfer(&w, id, a as u32).await,
        27 => return buy(&w, id, a != 0, b != 0).await,
        30 => {
            let world = w.read(0, 0, 0);
            let p = w.owner(world[5] as u32);
            return result(
                0,
                i64::from(a <= 0 || world[6] != 0 || p.is_null() || a <= get!(p, money)),
                255,
                0,
            );
        }
        31 => return result(0, available(&w, id), 255, 0),
        32 => return result(0, maxloan(w.owner(id)), 255, 0),
        35 => {
            if w.read(0, 0, 0)[9] != 0 {
                inflation(&w, true);
                recompute(&w);
            }
        }
        34 => {
            w.call(Service::AbortCompetitorTimer, 0, 0, 0, 0, 0);
        }
        29 => unsafe {
            TICK = 0;
        },
        _ => unreachable!(),
    }
    result(0, 0, 255, 0)
}

/// Initialize the four canonical construction budgets at Company construction.
/// # Safety
/// State is a live exclusive finance owner whose C++ view lifetime has started.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_company_initialize(
    p: *mut Finances,
    t: u32,
    c: u32,
    tree: u32,
    o: u32,
) {
    put!(p, t.wrapping_shl(16), terraform);
    put!(p, c.wrapping_shl(16), clear);
    put!(p, tree.wrapping_shl(16), tree);
    put!(p, o.wrapping_shl(16), object);
}

pub fn abi_layout(type_id: u8, item: u8) -> usize {
    use std::mem::{align_of, offset_of, size_of};
    macro_rules! layout {($t:ty,$($field:ident),+)=>{match item{0=>size_of::<$t>(),1=>align_of::<$t>(),_=>[$(offset_of!($t,$field)),+].get(usize::from(item)-2).copied().unwrap_or(usize::MAX)}};}
    match type_id {
        210 => layout!(Entry, income, expenses, cargo, performance, value),
        211 => {
            layout!(
                Finances,
                money,
                fraction,
                loan,
                max_loan,
                preview,
                empty,
                bankruptcy,
                asked,
                timeout,
                bankrupt_value,
                terraform,
                clear,
                tree,
                object,
                expenses,
                current,
                old,
                valid
            )
        }
        212 => {
            layout!(
                Economy,
                max_loan,
                fluct,
                interest,
                infl_amount,
                infl_payment,
                industry_counter,
                industry_increment,
                prices,
                payment,
                old_loan,
                old_fraction
            )
        }
        213 => layout!(Action, kind, id, a, b, c, d),
        214 => layout!(Leaves, next, read, owner, service),
        _ => usize::MAX,
    }
}
