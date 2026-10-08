/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Orders own their contiguous storage, lifecycle and timetable state. C++
//! placement-constructs typed objects before exposing facade aliases. All accesses
//! to aliased fields are raw, field-sized reads/writes: no Rust references to live
//! C++ Order objects survive external services, mutation or reentry. Panics abort.
// Native-width casts and branch structure reproduce the original order commands.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::too_many_arguments,
    clippy::similar_names,
    clippy::many_single_char_names,
    clippy::items_after_statements,
    clippy::if_not_else,
    clippy::collapsible_match
)]
use std::ffi::c_void;
use std::mem::MaybeUninit;
use std::ptr;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Order {
    pub kind: u8,
    pub flags: u8,
    pub destination: u16,
    pub refit: u8,
    pub wait: u16,
    pub travel: u16,
    pub speed: u16,
}
#[repr(C)]
pub struct Consist {
    pub time: i32,
    pub lateness: i32,
    pub start: u64,
    pub last_departure: u64,
    pub next_departure: u64,
    pub round_trip: i32,
    pub real: u8,
    pub implicit: u8,
    pub flags: u16,
}
#[repr(C)]
pub struct VehicleOrders {
    pub current: MaybeUninit<Order>,
    pub orders: *mut c_void,
    pub next: *mut c_void,
    pub previous: *mut c_void,
}
#[repr(C)]
pub struct List {
    pub manual: u8,
    pub vehicles: u32,
    pub first: *mut c_void,
    pub timetable: i32,
    pub total: i32,
}
#[repr(C)]
pub struct Backup {
    pub user: u32,
    pub tile: u32,
    pub group: u16,
    pub clone: *const c_void,
}

pub struct Vector {
    storage: Box<[MaybeUninit<Order>]>,
    length: usize,
    construct: unsafe extern "C" fn(*mut c_void, usize),
    destroy: unsafe extern "C" fn(*mut c_void, usize),
}
impl Vector {
    fn new(
        construct: unsafe extern "C" fn(*mut c_void, usize),
        destroy: unsafe extern "C" fn(*mut c_void, usize),
    ) -> Self {
        Self {
            storage: Box::new([]),
            length: 0,
            construct,
            destroy,
        }
    }
    fn data(&self) -> *mut Order {
        if self.storage.is_empty() {
            ptr::null_mut()
        } else {
            self.storage.as_ptr().cast_mut().cast()
        }
    }
    unsafe fn reserve(&mut self, required: usize) {
        if required <= self.storage.len() {
            return;
        }
        // Match the original standard-library growth policy on supported targets.
        #[cfg(target_env = "msvc")]
        let capacity = required.max(self.storage.len() + self.storage.len() / 2);
        #[cfg(not(target_env = "msvc"))]
        let capacity = required.max(self.length + self.length.max(1));
        let mut storage = Box::<[Order]>::new_uninit_slice(capacity);
        // SAFETY: New aligned storage is exclusive; the leaf begins C++ Order
        // lifetimes, including each DestinationID, without running simulation.
        unsafe {
            (self.construct)(storage.as_mut_ptr().cast(), capacity);
        }
        // SAFETY: Both ranges contain live trivially copyable C++ Order objects.
        // Existing values are copied before their source lifetimes end.
        unsafe {
            if self.length != 0 {
                ptr::copy_nonoverlapping(self.data(), storage.as_mut_ptr().cast(), self.length);
            }
            (self.destroy)(self.data().cast(), self.storage.len());
        }
        self.storage = storage;
    }
    unsafe fn resize(&mut self, size: usize) {
        unsafe {
            self.reserve(size);
        }
        if size < self.length {
            // Each vacated slot is reinitialized for subsequent vector growth.
            // Order is trivially destructible; no selected gameplay occurs here.
            unsafe {
                (self.destroy)(self.data().add(size).cast(), self.length - size);
            }
            unsafe {
                (self.construct)(self.data().add(size).cast(), self.length - size);
            }
        }
        self.length = size;
    }
}
impl Drop for Vector {
    fn drop(&mut self) {
        // SAFETY: No access remains when the shell destroys its unique owner.
        unsafe {
            (self.destroy)(self.data().cast(), self.storage.len());
        }
    }
}

/// All owner handles below are live, uniquely owned shell allocations. C++ does
/// not free Rust allocations; deletion happens once after typed access has ended.
/// Exports never unwind. Scalars have the original defaults and widths.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_consist_new() -> *mut Consist {
    Box::into_raw(Box::new(Consist {
        time: 0,
        lateness: 0,
        start: 0,
        last_departure: 0,
        next_departure: 0,
        round_trip: 0,
        real: 0,
        implicit: 0,
        flags: 0,
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consist_delete(p: *mut Consist) {
    unsafe {
        drop(Box::from_raw(p));
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consist_reset(p: *mut Consist) {
    unsafe {
        (*p).last_departure = 0;
        (*p).next_departure = 0;
        (*p).round_trip = 0;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consist_copy(p: *mut Consist, src: *const Consist) {
    if ptr::eq(p, src) {
        return;
    }
    unsafe {
        (*p).time = (*src).time;
        (*p).lateness = (*src).lateness;
        (*p).start = (*src).start;
        (*p).real = (*src).real;
        (*p).implicit = (*src).implicit;
        (*p).flags |= (*src).flags & ((1 << 3) | (1 << 4) | (1 << 5) | (1 << 8));
        (*p).flags = ((*p).flags & !(1 << 9)) | ((*src).flags & (1 << 9));
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_vehicle_orders_new() -> *mut VehicleOrders {
    Box::into_raw(Box::new(VehicleOrders {
        current: MaybeUninit::uninit(),
        orders: ptr::null_mut(),
        next: ptr::null_mut(),
        previous: ptr::null_mut(),
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_vehicle_orders_delete(p: *mut VehicleOrders) {
    unsafe {
        drop(Box::from_raw(p));
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_orderlist_new() -> *mut List {
    Box::into_raw(Box::new(List {
        manual: 0,
        vehicles: 0,
        first: ptr::null_mut(),
        timetable: 0,
        total: 0,
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orderlist_delete(p: *mut List) {
    unsafe {
        drop(Box::from_raw(p));
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_orderbackup_new() -> *mut Backup {
    Box::into_raw(Box::new(Backup {
        user: 0,
        tile: u32::MAX,
        group: u16::MAX,
        clone: ptr::null(),
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orderbackup_delete(p: *mut Backup) {
    unsafe {
        drop(Box::from_raw(p));
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_order_vector_new(
    construct: unsafe extern "C" fn(*mut c_void, usize),
    destroy: unsafe extern "C" fn(*mut c_void, usize),
) -> *mut Vector {
    Box::into_raw(Box::new(Vector::new(construct, destroy)))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_delete(p: *mut Vector) {
    unsafe {
        drop(Box::from_raw(p));
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_size(p: *const Vector) -> usize {
    unsafe { (*p).length }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_data(p: *const Vector) -> *mut Order {
    unsafe { (*p).data() }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_resize(p: *mut Vector, size: usize) {
    unsafe {
        (*p).resize(size);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_insert(
    p: *mut Vector,
    index: usize,
    src: *const Order,
) {
    // Copy before reserve: original emplace permits its input to alias a live order.
    let order = unsafe { src.read() };
    let v = unsafe { &mut *p };
    let index = index.min(v.length);
    unsafe {
        v.reserve(v.length + 1);
        ptr::copy(
            v.data().add(index),
            v.data().add(index + 1),
            v.length - index,
        );
        v.data().add(index).write(order);
    }
    v.length += 1;
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_erase(p: *mut Vector, index: usize) {
    let v = unsafe { &mut *p };
    if index >= v.length {
        return;
    }
    unsafe {
        ptr::copy(
            v.data().add(index + 1),
            v.data().add(index),
            v.length - index - 1,
        );
        v.resize(v.length - 1);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_move(p: *mut Vector, from: usize, to: usize) {
    let v = unsafe { &mut *p };
    if from == to || from >= v.length || to >= v.length {
        return;
    }
    unsafe {
        let order = v.data().add(from).read();
        if from < to {
            ptr::copy(v.data().add(from + 1), v.data().add(from), to - from);
        } else {
            ptr::copy(v.data().add(to), v.data().add(to + 1), from - to);
        }
        v.data().add(to).write(order);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_vector_assign(
    p: *mut Vector,
    src: *const Order,
    length: usize,
) {
    let v = unsafe { &mut *p };
    unsafe {
        v.resize(length);
        if length != 0 {
            ptr::copy_nonoverlapping(src, v.data(), length);
        }
    }
}

/// Shared leaves expose typed world shells; all pointed-to fields are accessed
/// only as raw scalars. Query/write are direct noexcept services, never selected
/// processing loops. Commands and destruction use separately named return actions.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub vehicle: unsafe extern "C" fn(*mut c_void) -> *mut VehicleOrders,
    pub consist: unsafe extern "C" fn(*mut c_void) -> *mut Consist,
    pub list: unsafe extern "C" fn(*mut c_void) -> *mut List,
    pub vector: unsafe extern "C" fn(*mut c_void) -> *mut Vector,
    pub backup: unsafe extern "C" fn(*mut c_void) -> *mut Backup,
    pub backup_vector: unsafe extern "C" fn(*mut c_void) -> *mut Vector,
    pub backup_consist: unsafe extern "C" fn(*mut c_void) -> *mut Consist,
    pub query: unsafe extern "C" fn(u32, *mut c_void, u64, u64, u64) -> u64,
    pub write: unsafe extern "C" fn(u32, *mut c_void, u64, u64, u64),
}
#[derive(Clone, Copy)]
struct Game(Leaves);
impl Game {
    fn query(self, op: u32, v: *mut c_void, a: u64, b: u64, c: u64) -> u64 {
        // SAFETY: Copied call-scoped shell context and scalar arguments, no borrow.
        unsafe { (self.0.query)(op, v, a, b, c) }
    }
    fn write(self, op: u32, v: *mut c_void, a: u64, b: u64, c: u64) {
        // SAFETY: Copied arguments only; no world or owner reference spans service.
        unsafe {
            (self.0.write)(op, v, a, b, c);
        }
    }
    fn vehicle(self, v: *mut c_void) -> *mut VehicleOrders {
        unsafe { (self.0.vehicle)(v) }
    }
    fn consist(self, v: *mut c_void) -> *mut Consist {
        unsafe { (self.0.consist)(v) }
    }
    fn list(self, list: *mut c_void) -> *mut List {
        unsafe { (self.0.list)(list) }
    }
    fn vector(self, list: *mut c_void) -> *mut Vector {
        unsafe { (self.0.vector)(list) }
    }
    fn orders(self, v: *mut c_void) -> *mut c_void {
        unsafe { (*self.vehicle(v)).orders }
    }
    fn next(self, v: *mut c_void) -> *mut c_void {
        unsafe { (*self.vehicle(v)).next }
    }
    fn previous(self, v: *mut c_void) -> *mut c_void {
        unsafe { (*self.vehicle(v)).previous }
    }
    fn first(self, v: *mut c_void) -> *mut c_void {
        let list = self.orders(v);
        if list.is_null() {
            self.query(1, v, 0, 0, 0) as usize as *mut c_void
        } else {
            unsafe { (*self.list(list)).first }
        }
    }
    fn size(self, list: *mut c_void) -> u8 {
        unsafe { (*self.vector(list)).length as u8 }
    }
    fn count(self, v: *mut c_void) -> u8 {
        let list = self.orders(v);
        if list.is_null() { 0 } else { self.size(list) }
    }
    fn stored_count(self, list: *mut c_void) -> usize {
        unsafe { (*self.vector(list)).length }
    }
    fn stored_order(self, list: *mut c_void, index: usize) -> Order {
        unsafe { (*self.vector(list)).data().add(index).read() }
    }
    fn put_stored(self, list: *mut c_void, index: usize, order: Order) {
        unsafe {
            (*self.vector(list)).data().add(index).write(order);
        }
    }
    fn order(self, list: *mut c_void, index: u8) -> Option<Order> {
        if index >= self.size(list) {
            None
        } else {
            unsafe { Some((*self.vector(list)).data().add(usize::from(index)).read()) }
        }
    }
    fn vehicle_order(self, v: *mut c_void, index: u8) -> Option<Order> {
        let list = self.orders(v);
        if list.is_null() {
            None
        } else {
            self.order(list, index)
        }
    }
    fn put_order(self, list: *mut c_void, index: u8, order: Order) {
        unsafe {
            (*self.vector(list))
                .data()
                .add(usize::from(index))
                .write(order);
        }
    }
    fn next_index(self, list: *mut c_void, index: u8) -> u8 {
        let count = self.size(list);
        if count == 0 {
            u8::MAX
        } else {
            ((u16::from(index) + 1) % u16::from(count)) as u8
        }
    }
    fn decision(self, list: *mut c_void, next: u8, hops: u32) -> u8 {
        if hops > u32::from(self.size(list)) || next >= self.size(list) {
            return u8::MAX;
        }
        let order = self.order(list, next).unwrap();
        if order.kind() == 7 {
            if order.variable() != 5 {
                return next;
            }
            return self.decision(list, order.flags, hops.wrapping_add(1));
        }
        if order.kind() == 2 {
            if order.action() & 1 != 0 {
                return u8::MAX;
            }
            if order.is_refit() {
                return next;
            }
        }
        if !order.can_load_unload() {
            return self.decision(list, self.next_index(list, next), hops.wrapping_add(1));
        }
        next
    }
    fn stopping(
        self,
        list: *mut c_void,
        v: *mut c_void,
        first: u8,
        mut hops: u32,
        output: *mut c_void,
    ) {
        let mut next = first;
        if first == u8::MAX {
            next = unsafe { (*self.consist(v)).implicit };
            if next >= self.size(list) {
                next = if self.size(list) == 0 { u8::MAX } else { 0 };
                if next == u8::MAX {
                    return;
                }
            } else {
                next = self.next_index(list, next);
            }
        }
        loop {
            hops = hops.wrapping_add(1);
            next = self.decision(list, next, hops);
            while next != u8::MAX && self.order(list, next).unwrap().kind() == 7 {
                let skip = self.decision(list, self.order(list, next).unwrap().flags, hops);
                let advance = self.decision(list, self.next_index(list, next), hops);
                if advance == u8::MAX || advance == first || skip == advance {
                    next = if skip == first { u8::MAX } else { skip };
                } else if skip == u8::MAX || skip == first {
                    next = if advance == first { u8::MAX } else { advance };
                } else {
                    self.stopping(list, v, skip, hops, output);
                    self.stopping(list, v, advance, hops, output);
                    return;
                }
                hops = hops.wrapping_add(1);
            }
            if next == u8::MAX {
                return;
            }
            let order = self.order(list, next).unwrap();
            let last = self.query(2, v, 0, 0, 0) as u16;
            if (order.kind() == 1 || order.kind() == 8)
                && order.destination == last
                && matches!(order.unload(), 1 | 2)
            {
                return;
            }
            if order.kind() == 2 || order.destination == last {
                continue;
            }
            self.write(1, output, u64::from(order.destination), 0, 0);
            return;
        }
    }
}
impl Order {
    fn kind(self) -> u8 {
        self.kind & 15
    }
    fn non_stop(self) -> u8 {
        self.kind >> 6
    }
    fn load(self) -> u8 {
        (self.flags >> 4) & 7
    }
    fn unload(self) -> u8 {
        self.flags & 7
    }
    fn action(self) -> u8 {
        (self.flags >> 3) & 15
    }
    fn depot_type(self) -> u8 {
        self.flags & 7
    }
    fn variable(self) -> u8 {
        (self.destination >> 11) as u8
    }
    fn comparator(self) -> u8 {
        self.kind >> 5
    }
    fn value(self) -> u16 {
        self.destination & 2047
    }
    fn is_refit(self) -> bool {
        self.refit < 64 || self.refit == 0xfd
    }
    fn wait_timetabled(self) -> bool {
        if self.kind() == 7 {
            self.wait > 0
        } else {
            self.flags & 8 != 0
        }
    }
    fn travel_timetabled(self) -> bool {
        if self.kind() == 7 {
            self.travel > 0
        } else {
            self.flags & 128 != 0
        }
    }
    fn timetabled_wait(self) -> u16 {
        if self.wait_timetabled() { self.wait } else { 0 }
    }
    fn timetabled_travel(self) -> u16 {
        if self.travel_timetabled() {
            self.travel
        } else {
            0
        }
    }
    fn can_load_unload(self) -> bool {
        if (self.kind() != 1 && self.kind() != 8) || self.non_stop() & 2 != 0 {
            return false;
        }
        !(self.load() == 4 && self.unload() == 4)
    }
    fn complete(self) -> bool {
        if !self.travel_timetabled() && self.kind() != 7 {
            return false;
        }
        !(!self.wait_timetabled() && self.kind() == 1 && self.non_stop() & 2 == 0)
    }
    fn equals(self, other: Self) -> bool {
        if self.kind() == 2 && self.kind == other.kind && (self.action() | other.action()) & 2 != 0
        {
            return self.depot_type() == other.depot_type()
                && self.action() & !2 == other.action() & !2;
        }
        self.kind == other.kind
            && self.flags == other.flags
            && self.destination == other.destination
    }
    fn set_wait_timetabled(&mut self, b: bool) {
        if self.kind() != 7 {
            self.flags = (self.flags & !8) | if b { 8 } else { 0 };
        }
    }
    fn set_travel_timetabled(&mut self, b: bool) {
        if self.kind() != 7 {
            self.flags = (self.flags & !128) | if b { 128 } else { 0 };
        }
    }
}

/// List operations own traversal and bookkeeping. Operation 2 returns whether
/// the C++ identity shell must be deleted, after this frame has returned.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orders_list(
    op: u32,
    list: *mut c_void,
    v: *mut c_void,
    a: u32,
    b: u32,
    input: *const Order,
    output: *mut c_void,
    leaves: *const Leaves,
) -> u32 {
    let g = Game(unsafe { leaves.read() });
    let state = g.list(list);
    match op {
        0 => {
            unsafe {
                (*state).first = v;
                (*state).manual = 0;
                (*state).vehicles = 1;
                (*state).timetable = 0;
            }
            for index in 0..g.stored_count(list) {
                let order = g.stored_order(list, index);
                unsafe {
                    if order.kind() != 8 {
                        (*state).manual = (*state).manual.wrapping_add(1);
                    }
                    (*state).total = (*state)
                        .total
                        .wrapping_add(i32::from(order.wait) + i32::from(order.travel));
                }
            }
            recalculate(g, list);
            let mut u = g.previous(v);
            while !u.is_null() {
                unsafe {
                    (*state).vehicles = (*state).vehicles.wrapping_add(1);
                    (*state).first = u;
                }
                u = g.previous(u);
            }
            u = g.next(v);
            while !u.is_null() {
                unsafe {
                    (*state).vehicles = (*state).vehicles.wrapping_add(1);
                }
                u = g.next(u);
            }
        }
        1 => recalculate(g, list),
        2 => {
            for index in 0..g.stored_count(list) {
                let order = g.stored_order(list, index);
                if matches!(order.kind(), 1 | 6)
                    && g.query(3, ptr::null_mut(), u64::from(order.destination), 1, 0) != 0
                {
                    g.write(2, ptr::null_mut(), 0, 0, 0);
                    break;
                }
            }
            if a == 0 {
                return 1;
            }
            unsafe {
                openttd_rust_order_vector_resize(g.vector(list), 0);
                (*state).manual = 0;
                (*state).timetable = 0;
            }
        }
        3 => return u32::from(g.decision(list, a as u8, b)),
        4 => g.stopping(list, v, a as u8, b, output),
        5 => {
            let order = unsafe { input.read() };
            unsafe {
                openttd_rust_order_vector_insert(g.vector(list), a as usize, input);
                if order.kind() != 8 {
                    (*state).manual = (*state).manual.wrapping_add(1);
                }
                (*state).timetable = (*state).timetable.wrapping_add(
                    i32::from(order.timetabled_wait()) + i32::from(order.timetabled_travel()),
                );
                (*state).total = (*state)
                    .total
                    .wrapping_add(i32::from(order.wait) + i32::from(order.travel));
            }
            if matches!(order.kind(), 1 | 6)
                && g.query(3, ptr::null_mut(), u64::from(order.destination), 0, 0) != 0
            {
                g.write(2, ptr::null_mut(), 0, 0, 0);
            }
        }
        6 => {
            if (a as usize) < g.stored_count(list) {
                let order = g.stored_order(list, a as usize);
                unsafe {
                    if order.kind() != 8 {
                        (*state).manual = (*state).manual.wrapping_sub(1);
                    }
                    (*state).timetable = (*state).timetable.wrapping_sub(
                        i32::from(order.timetabled_wait()) + i32::from(order.timetabled_travel()),
                    );
                    (*state).total = (*state)
                        .total
                        .wrapping_sub(i32::from(order.wait) + i32::from(order.travel));
                    openttd_rust_order_vector_erase(g.vector(list), a as usize);
                }
            }
        }
        7 => {
            if a < u32::from(g.size(list)) && b < u32::from(g.size(list)) {
                unsafe {
                    openttd_rust_order_vector_move(g.vector(list), a as usize, b as usize);
                }
            }
        }
        8 => unsafe {
            (*state).vehicles = (*state).vehicles.wrapping_sub(1);
            if (*state).first == v {
                (*state).first = g.next(v);
            }
        },
        9 => {
            return u32::from((0..g.stored_count(list)).all(|i| {
                let order = g.stored_order(list, i);
                order.kind() == 8 || order.complete()
            }));
        }
        _ => unreachable!(),
    }
    0
}
fn recalculate(g: Game, list: *mut c_void) {
    let state = g.list(list);
    unsafe {
        (*state).timetable = 0;
    }
    for i in 0..g.stored_count(list) {
        let order = g.stored_order(list, i);
        unsafe {
            (*state).timetable = (*state).timetable.wrapping_add(
                i32::from(order.timetabled_wait()) + i32::from(order.timetabled_travel()),
            );
        }
    }
}

impl Game {
    fn current(self, v: *mut c_void) -> Order {
        unsafe { (*self.vehicle(v)).current.as_ptr().read() }
    }
    fn put_current(self, v: *mut c_void, o: Order) {
        unsafe {
            (*self.vehicle(v)).current.as_mut_ptr().write(o);
        }
    }
    fn complete(self, list: *mut c_void) -> bool {
        (0..self.stored_count(list)).all(|i| {
            let o = self.stored_order(list, i);
            o.kind() == 8 || o.complete()
        })
    }
    fn duration(self, list: *mut c_void) -> i32 {
        if self.complete(list) {
            unsafe { (*self.list(list)).timetable }
        } else {
            -1
        }
    }
    fn reset(self, v: *mut c_void) {
        unsafe {
            openttd_rust_consist_reset(self.consist(v));
        }
    }
    fn dirty(self, v: *mut c_void) {
        self.write(4, v, 0, 0, 0);
    }
    fn change_timetable(self, v: *mut c_void, index: u8, val: u16, field: u8, timetabled: bool) {
        let list = self.orders(v);
        let mut order = self.order(list, index).unwrap();
        let (total_delta, timetable_delta) = match field {
            0 => {
                let deltas = (
                    i32::from(val) - i32::from(order.wait),
                    if timetabled { i32::from(val) } else { 0 }
                        - i32::from(order.timetabled_wait()),
                );
                order.wait = val;
                order.set_wait_timetabled(timetabled);
                deltas
            }
            1 => {
                let deltas = (
                    i32::from(val) - i32::from(order.travel),
                    if timetabled { i32::from(val) } else { 0 }
                        - i32::from(order.timetabled_travel()),
                );
                order.travel = val;
                order.set_travel_timetabled(timetabled);
                deltas
            }
            2 => {
                order.speed = val;
                (0, 0)
            }
            _ => unreachable!(),
        };
        self.put_order(list, index, order);
        let owner = self.list(list);
        unsafe {
            (*owner).total = (*owner).total.wrapping_add(total_delta);
            (*owner).timetable = (*owner).timetable.wrapping_add(timetable_delta);
        }
        let mut u = self.first(v);
        while !u.is_null() {
            let state = self.consist(u);
            let mut current = self.current(u);
            if unsafe { (*state).real == index } && current.equals(order) {
                match field {
                    0 => {
                        current.wait = val;
                        current.set_wait_timetabled(timetabled);
                    }
                    1 => {
                        current.travel = val;
                        current.set_travel_timetabled(timetabled);
                    }
                    2 => current.speed = val,
                    _ => unreachable!(),
                }
                self.put_current(u, current);
            }
            self.dirty(u);
            u = self.next(u);
        }
    }
    fn update_timetable(self, v: *mut c_void, travelling: bool) {
        let state = self.consist(v);
        let time = unsafe { (*state).time };
        unsafe {
            (*state).time = 0;
        }
        if self.current(v).kind() == 8 {
            return;
        }
        let real = unsafe { (*state).real };
        if real >= self.count(v) {
            return;
        }
        let mut order = self.vehicle_order(v, real).unwrap();
        let mut first = 0u8;
        let list = self.orders(v);
        for i in 0..self.stored_count(list) {
            if self.stored_order(list, i).kind() != 8 {
                break;
            }
            first = first.wrapping_add(1);
        }
        let mut just_started = false;
        if real == first && travelling {
            just_started = unsafe { (*state).flags & (1 << 3) == 0 };
            if unsafe { (*state).start != 0 } {
                unsafe {
                    (*state).lateness =
                        self.query(6, ptr::null_mut(), 0, 0, 0)
                            .wrapping_sub((*state).start) as i32;
                    (*state).start = 0;
                }
            }
            unsafe {
                (*state).flags |= 1 << 3;
            }
            self.dirty(v);
        }
        if unsafe { (*state).flags & (1 << 3) == 0 } {
            return;
        }
        let autofill = unsafe { (*state).flags & (1 << 4) != 0 };
        let remeasure =
            !order.wait_timetabled() || (autofill && unsafe { (*state).flags & (1 << 5) == 0 });
        if travelling && remeasure {
            let mut current = self.current(v);
            current.wait = 0;
            self.put_current(v, current);
        }
        if just_started {
            return;
        }
        if order.kind() != 7 && (travelling || time > i32::from(order.wait) || remeasure) {
            let seconds = self.query(10, ptr::null_mut(), 0, 0, 0) as i32;
            let value =
                (time.max(1).wrapping_add(seconds - 1) / seconds).wrapping_mul(seconds) as u16;
            if travelling && (autofill || !order.travel_timetabled()) {
                self.change_timetable(v, real, value, 1, autofill);
            } else if !travelling && (autofill || !order.wait_timetabled()) {
                self.change_timetable(v, real, value, 0, autofill);
            }
            order = self.vehicle_order(v, real).unwrap();
        }
        if real == first && travelling {
            unsafe {
                (*state).flags &= !((1 << 4) | (1 << 5));
            }
        }
        if autofill {
            return;
        }
        let timetable = i32::from(if travelling {
            order.timetabled_travel()
        } else {
            order.timetabled_wait()
        });
        if timetable == 0 && (travelling || unsafe { (*state).lateness >= 0 }) {
            return;
        }
        unsafe {
            (*state).lateness = (*state).lateness.wrapping_sub(timetable.wrapping_sub(time));
        }
        if unsafe { (*state).lateness > timetable } {
            let cycle = self.duration(self.orders(v));
            if cycle != -1 && unsafe { (*state).lateness > cycle } {
                unsafe {
                    (*state).lateness %= cycle;
                }
            }
        }
        let mut u = self.first(v);
        while !u.is_null() {
            self.dirty(u);
            u = self.next(u);
        }
    }
    fn owner_check(self, v: *mut c_void, output: *mut c_void) -> bool {
        self.query(9, v, output as usize as u64, 0, 0) != 0
    }
    fn command_error(self, output: *mut c_void, error: u32) -> u32 {
        self.write(3, output, u64::from(error), 0, 0);
        0
    }
    fn primary(self, v: *mut c_void) -> bool {
        !v.is_null() && self.query(7, v, 0, 0, 0) != 0
    }
    fn timetable_sort(self, a: *mut c_void, b: *mut c_void) -> std::cmp::Ordering {
        let sa = self.consist(a);
        let sb = self.consist(b);
        let mut ai = unsafe { (*sa).real };
        let mut bi = unsafe { (*sb).real };
        let raw = i32::from(bi) - i32::from(ai);
        let ao = self.current(a);
        let bo = self.current(b);
        if ao.kind() != 3 || ao.non_stop() == 0 {
            ai = ai.wrapping_sub(1);
        }
        if bo.kind() != 3 || bo.non_stop() == 0 {
            bi = bi.wrapping_sub(1);
        }
        let index = i32::from(bi) - i32::from(ai);
        if index != 0 {
            return index.cmp(&0);
        }
        if raw != 0 {
            return raw.cmp(&0);
        }
        let time = unsafe { (*sb).time.wrapping_sub((*sa).time) };
        if time != 0 {
            return time.cmp(&0);
        }
        self.query(11, b, 0, 0, 0).cmp(&self.query(11, a, 0, 0, 0))
    }
}

/// Timetable command validation and execution. Ownership is a shared company
/// service. Error IDs are mapped to unchanged strings by the C++ command facade.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orders_timetable(
    kind: u32,
    v: *mut c_void,
    execute: u8,
    a: u64,
    b: u64,
    c: u64,
    output: *mut c_void,
    leaves: *const Leaves,
) -> u32 {
    let g = Game(unsafe { leaves.read() });
    if kind == 5 {
        g.update_timetable(v, a != 0);
        return 1;
    }
    if !g.primary(v) {
        return g.command_error(output, 0);
    }
    if matches!(kind, 2..=4) && g.orders(v).is_null() {
        return g.command_error(output, 0);
    }
    let state = g.consist(v);
    if kind == 2 && a == 0 && unsafe { (*state).flags & (1 << 3) == 0 } {
        return g.command_error(output, 3);
    }
    if !g.owner_check(v, output) {
        return 0;
    }
    match kind {
        0 => {
            let index = a as u8;
            let field = b as u8;
            let Some(order) = g.vehicle_order(v, index) else {
                return g.command_error(output, 0);
            };
            if order.kind() == 8 || field >= 3 {
                return g.command_error(output, 0);
            }
            let mut wait = order.wait;
            let mut travel = order.travel;
            let mut speed = order.speed;
            match field {
                0 => wait = c as u16,
                1 => travel = c as u16,
                2 => speed = if c as u16 == 0 { u16::MAX } else { c as u16 },
                _ => unreachable!(),
            }
            if wait != order.wait {
                match order.kind() {
                    1 => {
                        if order.non_stop() & 2 != 0 {
                            return g.command_error(output, 1);
                        }
                    }
                    7 => {}
                    _ => return g.command_error(output, 2),
                }
            }
            if travel != order.travel && order.kind() == 7 {
                return g.command_error(output, 0);
            }
            if speed != order.speed && (order.kind() == 7 || g.query(4, v, 0, 0, 0) == 3) {
                return g.command_error(output, 0);
            }
            if execute != 0 {
                match field {
                    0 => {
                        if wait != order.wait || (wait > 0 && !order.wait_timetabled()) {
                            g.change_timetable(v, index, wait, 0, wait > 0);
                        }
                    }
                    1 => {
                        if travel != order.travel || (travel > 0 && !order.travel_timetabled()) {
                            g.change_timetable(v, index, travel, 1, travel > 0);
                        }
                    }
                    2 => {
                        if speed != order.speed {
                            g.change_timetable(v, index, speed, 2, speed != u16::MAX);
                        }
                    }
                    _ => unreachable!(),
                }
                let mut u = g.first(v);
                while !u.is_null() {
                    g.reset(u);
                    u = g.next(u);
                }
            }
        }
        1 => {
            if a >= 3 || g.count(v) == 0 {
                return g.command_error(output, 0);
            }
        }
        2 => {
            if execute != 0 {
                if a != 0 {
                    let mut late = 0;
                    let mut u = g.first(v);
                    while !u.is_null() {
                        if unsafe { (*state).flags & (1 << 3) != 0 } {
                            let us = g.consist(u);
                            late = late.max(unsafe { (*us).lateness });
                            g.reset(u);
                        }
                        u = g.next(u);
                    }
                    if late > 0 {
                        u = g.first(v);
                        while !u.is_null() {
                            if unsafe { (*state).flags & (1 << 3) != 0 } {
                                let us = g.consist(u);
                                unsafe {
                                    (*us).lateness = (*us).lateness.wrapping_sub(late);
                                }
                                g.dirty(u);
                            }
                            u = g.next(u);
                        }
                    }
                } else {
                    unsafe {
                        (*state).lateness = 0;
                    }
                    g.reset(v);
                    g.dirty(v);
                }
            }
        }
        3 => {
            let list = g.orders(v);
            let total = g.duration(list);
            let start = b;
            let date = g.query(12, ptr::null_mut(), 0, 0, 0) as i32;
            let start_date = date.wrapping_add(
                (start.wrapping_sub(g.query(6, ptr::null_mut(), 0, 0, 0)) as i32)
                    .wrapping_add(g.query(13, ptr::null_mut(), 0, 0, 0) as i32)
                    / 74,
            );
            let max_date = g.query(14, ptr::null_mut(), 0, 0, 0) as i32;
            if start_date < 0 || start_date > max_date {
                return g.command_error(output, 0);
            }
            if start_date.wrapping_sub(date) > g.query(15, ptr::null_mut(), 0, 0, 0) as i32
                || date.wrapping_sub(start_date) > 366
            {
                return g.command_error(output, 0);
            }
            if a != 0 && !g.complete(list) {
                return g.command_error(output, 4);
            }
            if a != 0 && start_date.wrapping_add(total / 74) > max_date {
                return g.command_error(output, 0);
            }
            if execute != 0 {
                let mut vehicles = Vec::new();
                if a != 0 {
                    let mut u = unsafe { (*g.list(list)).first };
                    while !u.is_null() {
                        vehicles.push(u);
                        u = g.next(u);
                    }
                } else {
                    vehicles.push(v);
                }
                if vehicles.len() >= 2 {
                    vehicles.sort_unstable_by(|a, b| g.timetable_sort(*a, *b));
                }
                let count = vehicles.len() as i32;
                for (index, u) in vehicles.into_iter().enumerate() {
                    let us = g.consist(u);
                    unsafe {
                        (*us).lateness = 0;
                        (*us).flags &= !(1 << 3);
                        (*us).start =
                            start.wrapping_add(((index as i32).wrapping_mul(total) / count) as u64);
                    }
                    // Preserve the original v (rather than u) reset quirk.
                    g.reset(v);
                    g.dirty(u);
                }
            }
        }
        4 => {
            if execute != 0 {
                if a != 0 {
                    unsafe {
                        (*state).flags |= 1 << 4;
                        (*state).flags &= !(1 << 3);
                        if b != 0 {
                            (*state).flags |= 1 << 5;
                        }
                        (*state).start = 0;
                        (*state).lateness = 0;
                    }
                } else {
                    unsafe {
                        (*state).flags &= !((1 << 4) | (1 << 5));
                    }
                }
                let mut u = g.first(v);
                while !u.is_null() {
                    if u != v {
                        let us = g.consist(u);
                        unsafe {
                            (*us).flags &= !((1 << 4) | (1 << 5));
                        }
                    }
                    g.dirty(u);
                    u = g.next(u);
                }
            }
        }
        _ => unreachable!(),
    }
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_orders_start_tick(
    date: i32,
    current_date: i32,
    fract: u16,
    counter: u64,
) -> u64 {
    counter.wrapping_add(
        date.wrapping_sub(current_date)
            .wrapping_mul(74)
            .wrapping_sub(i32::from(fract)) as u64,
    )
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_orders_start_date(
    start: u64,
    date: i32,
    fract: u16,
    counter: u64,
) -> i32 {
    date.wrapping_add((start.wrapping_sub(counter) as i32).wrapping_add(i32::from(fract)) / 74)
}

impl Game {
    fn manual(self, v: *mut c_void) -> u8 {
        let list = self.orders(v);
        if list.is_null() {
            0
        } else {
            unsafe { (*self.list(list)).manual }
        }
    }
    fn update_real(self, v: *mut c_void) {
        let state = self.consist(v);
        if unsafe { (*state).real >= self.count(v) } {
            unsafe {
                (*state).real = 0;
            }
        }
        if self.manual(v) > 0 {
            while self
                .vehicle_order(v, unsafe { (*state).real })
                .unwrap()
                .kind()
                == 8
            {
                unsafe {
                    (*state).real = (*state).real.wrapping_add(1);
                    if (*state).real >= self.count(v) {
                        (*state).real = 0;
                    }
                }
            }
        } else {
            unsafe {
                (*state).real = 0;
            }
        }
    }
    fn skip_real(self, v: *mut c_void) {
        let state = self.consist(v);
        if self.manual(v) > 0 {
            loop {
                unsafe {
                    (*state).real = (*state).real.wrapping_add(1);
                    if (*state).real >= self.count(v) {
                        (*state).real = 0;
                    }
                }
                if self
                    .vehicle_order(v, unsafe { (*state).real })
                    .unwrap()
                    .kind()
                    != 8
                {
                    break;
                }
            }
        } else {
            unsafe {
                (*state).real = 0;
            }
        }
    }
    fn increment_implicit(self, v: *mut c_void) {
        let state = self.consist(v);
        if unsafe { (*state).implicit == (*state).real } {
            self.skip_real(v);
        }
        loop {
            unsafe {
                (*state).implicit = (*state).implicit.wrapping_add(1);
                if (*state).implicit >= self.count(v) {
                    (*state).implicit = 0;
                }
            }
            if unsafe { (*state).implicit == (*state).real }
                || self
                    .vehicle_order(v, unsafe { (*state).implicit })
                    .unwrap()
                    .kind()
                    == 8
            {
                break;
            }
        }
        self.write(5, v, 0, 0, 0);
    }
    fn increment_real(self, v: *mut c_void) {
        let state = self.consist(v);
        if unsafe { (*state).implicit == (*state).real } {
            self.increment_implicit(v);
        } else {
            self.skip_real(v);
            self.write(5, v, 0, 0, 0);
        }
    }
    fn has(self, v: *mut c_void, selector: u32) -> bool {
        let list = self.orders(v);
        if list.is_null() {
            return false;
        }
        (0..self.stored_count(list)).any(|i| {
            let o = self.stored_order(list, i);
            match selector {
                0 => o.kind() == 2,
                1 => o.kind() == 1 && matches!(o.load(), 2 | 3),
                2 => o.kind() == 7,
                3 => o.kind() == 2 && o.action() & 4 != 0,
                _ => unreachable!(),
            }
        })
    }
    fn previous_unbunching(self, v: *mut c_void) -> bool {
        let list = self.orders(v);
        if list.is_null() {
            return false;
        }
        let implicit = unsafe { (*self.consist(v)).implicit };
        let index = if (implicit >= self.count(v) && self.count(v) == 0) || implicit == 0 {
            self.count(v).wrapping_sub(1)
        } else {
            implicit.wrapping_sub(1)
        };
        self.vehicle_order(v, index)
            .is_some_and(|o| o.kind() == 2 && o.action() & 4 != 0)
    }
    fn leave_unbunching(self, v: *mut c_void) {
        if !self.previous_unbunching(v) {
            return;
        }
        let state = self.consist(v);
        let tick = self.query(6, ptr::null_mut(), 0, 0, 0);
        unsafe {
            (*state).last_departure = tick;
            (*state).lateness = 0;
        }
        self.dirty(v);
        let mut count = 0i32;
        let mut time = 0i32;
        let mut u = self.first(v);
        while !u.is_null() {
            if self.query(16, u, 0, 0, 0) == 0 {
                count = count.wrapping_add(1);
                time = time.wrapping_add(unsafe { (*self.consist(u)).round_trip });
            }
            u = self.next(u);
        }
        count = count.max(1);
        let separation = (time / count / count).max(1);
        let departure = tick.wrapping_add(separation as u64);
        u = self.first(v);
        while !u.is_null() {
            if self.query(16, u, 0, 0, 0) == 0 {
                unsafe {
                    (*self.consist(u)).next_departure = departure;
                }
                self.write(6, u, 0, 0, 0);
            }
            u = self.next(u);
        }
    }
    fn measure_unbunching(self, v: *mut c_void) {
        let state = self.consist(v);
        if self.current(v).action() & 4 == 0 || unsafe { (*state).last_departure == 0 } {
            return;
        }
        let measured = self
            .query(6, ptr::null_mut(), 0, 0, 0)
            .wrapping_sub(unsafe { (*state).last_departure }) as i32;
        let previous = unsafe { (*state).round_trip };
        // Original int32 product occurs before ClampTo<int32>; preserve native wrap.
        let result = if previous == 0 {
            measured
        } else {
            {
                let lower = previous / 2;
                let upper = previous.wrapping_mul(2);
                self.write(19, ptr::null_mut(), lower as u64, upper as u64, 0);
                if measured <= lower {
                    lower
                } else if measured >= upper {
                    upper
                } else {
                    measured
                }
            }
        };
        unsafe {
            (*state).round_trip = result;
        }
    }
}
/// Scalar vehicle policy, without any persistent state reference or world borrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orders_vehicle(
    op: u32,
    v: *mut c_void,
    a: u32,
    leaves: *const Leaves,
) -> u32 {
    let g = Game(unsafe { leaves.read() });
    match op {
        0 => g.update_real(v),
        1 => g.skip_real(v),
        2 => g.increment_implicit(v),
        3 => g.increment_real(v),
        4 => return u32::from(g.has(v, a)),
        5 => g.leave_unbunching(v),
        6 => {
            return u32::from(
                !g.orders(v).is_null()
                    && unsafe { (*g.list(g.orders(v))).vehicles > 1 }
                    && g.count(v) > 1
                    && g.previous_unbunching(v)
                    && unsafe {
                        (*g.consist(v)).next_departure > g.query(6, ptr::null_mut(), 0, 0, 0)
                    },
            );
        }
        7 => g.measure_unbunching(v),
        _ => unreachable!(),
    }
    0
}
/// All fields belong to a live, properly constructed C++ Order, accessed raw.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_order_scalar(
    op: u32,
    p: *mut Order,
    a: u32,
    b: u32,
    c: u32,
    d: u32,
    q: *const Order,
) -> u32 {
    let mut order = unsafe { p.read() };
    match op {
        0 => {
            order.kind = 0;
            order.flags = 0;
            order.destination = 0;
        }
        1 => {
            order.kind = 1;
            order.flags = 0;
            order.destination = a as u16;
        }
        2 => {
            order.kind = 2;
            order.flags = (order.flags & !7) | (b as u8 & 7);
            order.flags = (order.flags & !0x78) | ((d as u8 & 15) << 3);
            order.kind = (order.kind & !0xc0) | ((c as u8 & 3) << 6);
            order.destination = a as u16;
            order.refit = (d >> 8) as u8;
        }
        3 => {
            order.kind = 6;
            order.flags = 0;
            order.destination = a as u16;
        }
        4 => {
            order.kind = 3;
            if a == 0 {
                order.flags = 0;
            }
        }
        5 => {
            order.kind = 4;
            order.flags = 0;
        }
        6 => {
            order.kind = 5;
            order.flags = 0;
        }
        7 => {
            order.kind = 7;
            order.flags = a as u8;
            order.destination = 0;
        }
        8 => {
            order.kind = 8;
            order.destination = a as u16;
        }
        9 => order.refit = a as u8,
        10 => return u32::from(order.equals(unsafe { q.read() })),
        11 => order = unsafe { q.read() },
        12 => {
            let mut packed = u16::from(order.kind());
            match order.kind() {
                1 => {
                    if order.unload() == 1 {
                        packed |= 1 << 5;
                    }
                    if matches!(order.load(), 2 | 3) {
                        packed |= 1 << 6;
                    }
                    if order.non_stop() & 1 != 0 {
                        packed |= 1 << 7;
                    }
                    packed |= (order.destination & 255) << 8;
                }
                2 => {
                    if order.depot_type() & 2 == 0 {
                        packed |= 1 << 6;
                    }
                    packed |= 1 << 7;
                    packed |= (order.destination & 255) << 8;
                }
                3 => {
                    if matches!(order.load(), 2 | 3) {
                        packed |= 1 << 6;
                    }
                    if order.load() == 4 && order.unload() == 4 {
                        packed = 0;
                    }
                }
                _ => {}
            }
            return u32::from(packed);
        }
        13 => return u32::from(order.can_load_unload()),
        14 => {
            return u32::from(
                order.load() != 4 || (a != 0 && order.unload() != 1 && order.unload() != 2),
            );
        }
        15 => {
            return u32::from(
                (order.kind() != 2 || order.depot_type() & 2 != 0)
                    && a != b
                    && order.non_stop()
                        & (if order.kind() == 1 && u32::from(order.destination) == b {
                            2
                        } else {
                            1
                        })
                        == 0,
            );
        }
        _ => unreachable!(),
    }
    unsafe {
        p.write(order);
    }
    0
}

use std::cell::Cell;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context, Poll, Waker};
/// Named ordinary reentry points. No owner or order borrow is held on return.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Action {
    pub operation: u32,
    pub context: *mut c_void,
    pub a: u64,
    pub b: u64,
    pub c: u64,
}
impl Default for Action {
    fn default() -> Self {
        Self {
            operation: 0,
            context: ptr::null_mut(),
            a: 0,
            b: 0,
            c: 0,
        }
    }
}
#[derive(Default)]
struct Mailbox {
    action: Cell<Action>,
    response: Cell<u64>,
}
struct Reentry {
    mailbox: Rc<Mailbox>,
    action: Action,
    yielded: bool,
}
impl Future for Reentry {
    type Output = u64;
    fn poll(mut self: Pin<&mut Self>, _: &mut Context<'_>) -> Poll<u64> {
        if self.yielded {
            Poll::Ready(self.mailbox.response.get())
        } else {
            self.mailbox.action.set(self.action);
            self.yielded = true;
            Poll::Pending
        }
    }
}
struct Control {
    g: Game,
    mailbox: Rc<Mailbox>,
}
impl Control {
    async fn reentry(&self, operation: u32, context: *mut c_void, a: u64, b: u64, c: u64) -> u64 {
        Reentry {
            mailbox: self.mailbox.clone(),
            action: Action {
                operation,
                context,
                a,
                b,
                c,
            },
            yielded: false,
        }
        .await
    }
    fn initialize_list(&self, list: *mut c_void, v: *mut c_void) {
        unsafe {
            openttd_rust_orders_list(
                0,
                list,
                v,
                0,
                0,
                ptr::null(),
                ptr::null_mut(),
                &raw const self.g.0,
            );
        }
    }
    fn new_list(&self, v: *mut c_void, input: Option<Order>, source: *mut c_void) -> *mut c_void {
        let g = self.g;
        let list = g.query(17, ptr::null_mut(), 0, 0, 0) as usize as *mut c_void;
        if let Some(order) = input {
            unsafe {
                openttd_rust_order_vector_insert(g.vector(list), 0, &raw const order);
            }
        }
        if !source.is_null() {
            let vector = g.vector(source);
            let data = unsafe { (*vector).data() };
            let count = unsafe { (*vector).length };
            unsafe {
                openttd_rust_order_vector_assign(g.vector(list), data, count);
            }
        }
        self.initialize_list(list, v);
        list
    }
    fn insert(&self, v: *mut c_void, order: Order, index: u8) {
        let g = self.g;
        let list = g.orders(v);
        if list.is_null() {
            let list = self.new_list(v, Some(order), ptr::null_mut());
            unsafe {
                (*g.vehicle(v)).orders = list;
            }
        } else {
            unsafe {
                openttd_rust_orders_list(
                    5,
                    list,
                    ptr::null_mut(),
                    u32::from(index),
                    0,
                    &raw const order,
                    ptr::null_mut(),
                    &raw const g.0,
                );
            }
        }
        let mut u = g.first(v);
        g.write(7, u, 0, 0, 0);
        while !u.is_null() {
            let state = g.consist(u);
            if index <= unsafe { (*state).real } {
                let next = u16::from(unsafe { (*state).real }) + 1;
                if next < u16::from(g.count(u)) {
                    unsafe {
                        (*state).real = next as u8;
                    }
                }
            }
            if index == unsafe { (*state).implicit } && g.query(4, u, 0, 0, 0) < 2 {
                g.write(8, u, 1, 0, 0);
            }
            if index <= unsafe { (*state).implicit } {
                let next = u16::from(unsafe { (*state).implicit }) + 1;
                if next < u16::from(g.count(u)) {
                    unsafe {
                        (*state).implicit = next as u8;
                    }
                }
            }
            g.reset(u);
            g.write(
                5,
                u,
                u64::from(u16::from(u8::MAX) | (u16::from(index) << 8)),
                0,
                0,
            );
            u = g.next(u);
        }
        let list = g.orders(v);
        for i in 0..g.stored_count(list) {
            let mut o = g.stored_order(list, i);
            if o.kind() == 7 {
                let target = o.flags;
                if target >= index {
                    o.flags = target.wrapping_add(1);
                }
                if target == i as u8 {
                    o.flags = ((u16::from(target) + 1) % u16::from(g.count(v))) as u8;
                }
                g.put_stored(list, i, o);
            }
        }
        g.write(9, v, 0, 0, 0);
    }
    fn cancel_loading(&self, v: *mut c_void) {
        let mut order = self.g.current(v);
        order.kind &= !0xc0;
        if matches!(order.load(), 2 | 3) {
            order.flags &= !0x70;
        }
        self.g.put_current(v, order);
    }
    fn delete(&self, v: *mut c_void, index: u8) {
        let g = self.g;
        unsafe {
            openttd_rust_orders_list(
                6,
                g.orders(v),
                ptr::null_mut(),
                u32::from(index),
                0,
                ptr::null(),
                ptr::null_mut(),
                &raw const g.0,
            );
        }
        let mut u = g.first(v);
        g.write(7, u, 0, 0, 0);
        while !u.is_null() {
            let state = g.consist(u);
            if index == unsafe { (*state).real } && g.current(u).kind() == 3 {
                self.cancel_loading(u);
            }
            if index < unsafe { (*state).real } {
                unsafe {
                    (*state).real = (*state).real.wrapping_sub(1);
                }
            } else if index == unsafe { (*state).real } {
                g.update_real(u);
            }
            if index < unsafe { (*state).implicit } {
                unsafe {
                    (*state).implicit = (*state).implicit.wrapping_sub(1);
                }
            } else if index == unsafe { (*state).implicit } {
                if unsafe { (*state).implicit >= g.count(u) } {
                    unsafe {
                        (*state).implicit = 0;
                    }
                }
                while unsafe { (*state).implicit != (*state).real }
                    && g.vehicle_order(u, unsafe { (*state).implicit })
                        .unwrap()
                        .kind()
                        != 8
                {
                    unsafe {
                        (*state).implicit = (*state).implicit.wrapping_add(1);
                        if (*state).implicit >= g.count(u) {
                            (*state).implicit = 0;
                        }
                    }
                }
            }
            g.reset(u);
            g.write(
                5,
                u,
                u64::from(u16::from(index) | (u16::from(u8::MAX) << 8)),
                0,
                0,
            );
            u = g.next(u);
        }
        let list = g.orders(v);
        for i in 0..g.stored_count(list) {
            let mut o = g.stored_order(list, i);
            if o.kind() == 7 {
                let mut target = o.flags;
                if target >= index {
                    target = target.saturating_sub(1);
                }
                if target == i as u8 {
                    target = ((u16::from(target) + 1) % u16::from(g.count(v))) as u8;
                }
                o.flags = target;
                g.put_stored(list, i, o);
            }
        }
        g.write(9, v, 0, 0, 0);
    }
    fn delete_implicit(&self, v: *mut c_void) {
        let g = self.g;
        let state = g.consist(v);
        if g.query(4, v, 0, 0, 0) < 2 && g.query(18, v, 0, 0, 0) != 0 {
            g.write(8, v, 0, 0, 0);
            unsafe {
                (*state).implicit = (*state).real;
            }
            g.write(5, v, 0, 0, 0);
            return;
        }
        let mut current = unsafe { (*state).implicit };
        while current != u8::MAX {
            if unsafe { (*state).implicit == (*state).real } {
                break;
            }
            if g.vehicle_order(v, current).unwrap().kind() == 8 {
                self.delete(v, unsafe { (*state).implicit });
            } else {
                let next = g.next_index(g.orders(v), current);
                if current < next {
                    unsafe {
                        (*state).implicit = (*state).implicit.wrapping_add(1);
                    }
                } else {
                    unsafe {
                        (*state).implicit = 0;
                    }
                }
                current = next;
            }
        }
    }
    fn move_order(&self, v: *mut c_void, from: u8, to: u8) {
        let g = self.g;
        let list = g.orders(v);
        unsafe {
            openttd_rust_order_vector_move(g.vector(list), usize::from(from), usize::from(to));
        }
        let mut u = g.first(v);
        g.write(7, u, 0, 0, 0);
        fn remap(index: u8, from: u8, to: u8) -> u8 {
            if index == from {
                to
            } else if index > from && index <= to {
                index.wrapping_sub(1)
            } else if index < from && index >= to {
                index.wrapping_add(1)
            } else {
                index
            }
        }
        while !u.is_null() {
            let state = g.consist(u);
            unsafe {
                (*state).real = remap((*state).real, from, to);
                (*state).implicit = remap((*state).implicit, from, to);
            }
            g.reset(u);
            g.write(
                5,
                u,
                u64::from(u16::from(from) | (u16::from(to) << 8)),
                0,
                0,
            );
            u = g.next(u);
        }
        for i in 0..g.stored_count(list) {
            let mut o = g.stored_order(list, i);
            if o.kind() == 7 {
                o.flags = remap(o.flags, from, to);
                g.put_stored(list, i, o);
            }
        }
        g.write(9, v, 0, 0, 0);
    }
    fn add_shared(&self, v: *mut c_void, shared: *mut c_void) {
        let g = self.g;
        if g.orders(shared).is_null() {
            let list = self.new_list(shared, None, ptr::null_mut());
            unsafe {
                (*g.vehicle(v)).orders = list;
                (*g.vehicle(shared)).orders = list;
            }
        }
        let next = g.next(shared);
        unsafe {
            (*g.vehicle(v)).next = next;
            (*g.vehicle(v)).previous = shared;
            (*g.vehicle(shared)).next = v;
            if !next.is_null() {
                (*g.vehicle(next)).previous = v;
            }
        }
        let list = g.list(g.orders(shared));
        unsafe {
            (*list).vehicles = (*list).vehicles.wrapping_add(1);
        }
    }
    fn remove_shared(&self, v: *mut c_void) {
        let g = self.g;
        let first = g.first(v);
        let were_first = first == v;
        // Window number is copied before changing the head, as in the original.
        let identifier = g.query(19, v, first as usize as u64, 0, 0);
        let list = g.list(g.orders(v));
        unsafe {
            (*list).vehicles = (*list).vehicles.wrapping_sub(1);
            if (*list).first == v {
                (*list).first = g.next(v);
            }
        }
        let previous = g.previous(v);
        let next = g.next(v);
        if !were_first {
            unsafe {
                (*g.vehicle(previous)).next = next;
            }
        }
        if !next.is_null() {
            unsafe {
                (*g.vehicle(next)).previous = previous;
            }
        }
        if unsafe { (*list).vehicles == 1 } {
            g.write(10, v, identifier, 0, 0);
            g.write(5, g.first(v), (-2i64) as u64, 0, 0);
        } else if were_first {
            g.write(
                11,
                v,
                identifier,
                g.query(20, g.first(v), 0, 0, 0) | (1 << 31),
                0,
            );
        }
        unsafe {
            (*g.vehicle(v)).next = ptr::null_mut();
            (*g.vehicle(v)).previous = ptr::null_mut();
        }
    }
    async fn delete_vehicle_orders(&self, v: *mut c_void, keep: bool, reset: bool) {
        let g = self.g;
        g.write(7, v, 0, 0, 0);
        let list = g.orders(v);
        if !list.is_null() && unsafe { (*g.list(list)).vehicles > 1 } {
            self.remove_shared(v);
            unsafe {
                (*g.vehicle(v)).orders = ptr::null_mut();
            }
        } else if !list.is_null() {
            let remove = unsafe {
                openttd_rust_orders_list(
                    2,
                    list,
                    ptr::null_mut(),
                    u32::from(keep),
                    0,
                    ptr::null(),
                    ptr::null_mut(),
                    &raw const g.0,
                )
            };
            if remove != 0 {
                self.reentry(1, list, 0, 0, 0).await;
            }
            if !keep {
                unsafe {
                    (*g.vehicle(v)).orders = ptr::null_mut();
                }
            }
        }
        g.reset(v);
        if reset {
            let state = g.consist(v);
            unsafe {
                (*state).implicit = 0;
                (*state).real = 0;
            }
            if g.current(v).kind() == 3 {
                self.cancel_loading(v);
            }
        }
    }
}
struct Task {
    future: Pin<Box<dyn Future<Output = u64>>>,
    mailbox: Rc<Mailbox>,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orders_create(
    kind: u32,
    v: *mut c_void,
    a: u64,
    b: u64,
    c: u64,
    input: *const Order,
    output: *mut c_void,
    leaves: *const Leaves,
) -> *mut c_void {
    let mailbox = Rc::new(Mailbox::default());
    let control = Control {
        g: Game(unsafe { leaves.read() }),
        mailbox: mailbox.clone(),
    };
    let order = if input.is_null() {
        None
    } else {
        Some(unsafe { input.read() })
    };
    Box::into_raw(Box::new(Task {
        future: Box::pin(run(control, kind, v, a, b, c, order, output)),
        mailbox,
    }))
    .cast()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orders_advance(task: *mut c_void, response: u64) -> Action {
    let task = unsafe { &mut *task.cast::<Task>() };
    task.mailbox.response.set(response);
    let mut cx = Context::from_waker(Waker::noop());
    match task.future.as_mut().poll(&mut cx) {
        Poll::Ready(result) => Action {
            a: result,
            ..Action::default()
        },
        Poll::Pending => task.mailbox.action.get(),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_orders_task_delete(task: *mut c_void) {
    unsafe {
        drop(Box::from_raw(task.cast::<Task>()));
    }
}
async fn run(
    control: Control,
    kind: u32,
    v: *mut c_void,
    a: u64,
    b: u64,
    c: u64,
    input: Option<Order>,
    output: *mut c_void,
) -> u64 {
    let g = control.g;
    match kind {
        0 => control.insert(v, input.unwrap(), a as u8),
        1 => control.delete(v, a as u8),
        2 => control.move_order(v, a as u8, b as u8),
        3 => control.delete_vehicle_orders(v, a != 0, b != 0).await,
        4 => control.add_shared(v, a as usize as *mut c_void),
        5 => control.remove_shared(v),
        6 => control.delete_implicit(v),
        7 => {
            let valid =
                unsafe { openttd_rust_orders_timetable(1, v, 0, a, b, 0, output, &raw const g.0) };
            if valid == 0 {
                return 0;
            }
            if c != 0 {
                let mut index = 0u8;
                while index < g.count(v) {
                    let order = g.vehicle_order(v, index);
                    if order.is_some_and(|o| o.kind() != 8) {
                        control.reentry(8, v, u64::from(index), a, b).await;
                    }
                    index = index.wrapping_add(1);
                }
            }
            return 1;
        }
        8 => {
            return u64::from(
                control
                    .destination(v, input.unwrap(), a as i32, b != 0)
                    .await,
            );
        }
        9 => return u64::from(control.process(v).await),
        10 => return u64::from(g.conditional(input.unwrap(), v)),
        11 => return u64::from(g.location(input.unwrap(), v, a != 0)),
        12 => return u64::from(g.distance(a as u8, b as u8, v, c as i32)),
        13 => return control.insert_command(v, a as u8, input.unwrap(), b != 0, output),
        14 => return control.delete_command(v, a as u8, b != 0, output).await,
        15 => return control.skip_command(v, a as u8, b != 0, output).await,
        16 => return control.move_command(v, a as u8, b as u8, c != 0, output),
        17 => return control.modify_command(v, a as u8, b as u8, c as u16, c >> 32 != 0, output),
        18 => return control.refit_command(v, a as u8, b as u8, c != 0, output),
        19 => {
            return control
                .clone_command(v, a as usize as *mut c_void, b as u8, c != 0, output)
                .await;
        }
        20..=28 => return control.backup_operation(kind - 20, v, a, b, c).await,
        29 => {
            if c != 0 {
                control
                    .backup_operation(
                        3,
                        ptr::null_mut(),
                        if a == 0 { u64::from(u32::MAX) } else { a },
                        b,
                        0,
                    )
                    .await;
            }
            return 1;
        }
        30 => control.remove_destination(a as u8, b as u16, c != 0).await,
        31 => control.check_orders(v),
        32 => control.begin_loading_orders(v),
        _ => unreachable!(),
    }
    0
}

#[repr(C)]
#[derive(Default)]
pub struct Closest {
    pub tile: u32,
    pub destination: u16,
    pub reverse: u8,
    pub found: u8,
}
impl Game {
    fn conditional(self, order: Order, v: *mut c_void) -> u8 {
        if order.kind() != 7 {
            return u8::MAX;
        }
        let variable = match order.variable() {
            0 => self.query(21, v, 0, 0, 0) as i32,
            1 => ((self.query(22, v, 0, 0, 0) * 101) >> 16) as i32,
            7 => ((self.query(23, v, 0, 0, 0) * 101) >> 16) as i32,
            2 => (self.query(24, v, 0, 0, 0) as i32).wrapping_mul(10) / 16,
            3 => self.query(25, v, 0, 0, 0) as i32,
            4 => self.query(26, v, 0, 0, 0) as i32,
            5 => return order.flags,
            6 => self.query(27, v, 0, 0, 0) as i32,
            _ => unreachable!(),
        };
        let value = i32::from(order.value());
        let skip = match order.comparator() {
            0 => variable == value,
            1 => variable != value,
            2 => variable < value,
            3 => variable <= value,
            4 => variable > value,
            5 => variable >= value,
            6 => variable != 0,
            7 => variable == 0,
            _ => unreachable!(),
        };
        if skip { order.flags } else { u8::MAX }
    }
    fn location(self, order: Order, v: *mut c_void, airport: bool) -> u32 {
        match order.kind() {
            1 | 6 | 8 => self.query(
                if airport && self.query(4, v, 0, 0, 0) == 3 {
                    28
                } else {
                    29
                },
                ptr::null_mut(),
                u64::from(order.destination),
                0,
                0,
            ) as u32,
            2 => {
                if order.destination == u16::MAX {
                    return u32::MAX;
                }
                self.query(
                    if self.query(4, v, 0, 0, 0) == 3 {
                        30
                    } else {
                        31
                    },
                    ptr::null_mut(),
                    u64::from(order.destination),
                    0,
                    0,
                ) as u32
            }
            _ => u32::MAX,
        }
    }
    fn distance(self, previous: u8, current: u8, v: *mut c_void, depth: i32) -> u32 {
        let list = self.orders(v);
        let order = self.order(list, current).unwrap();
        if order.kind() == 7 {
            if depth > i32::from(self.count(v)) {
                return 0;
            }
            let d1 = self.distance(previous, order.flags, v, depth.wrapping_add(1)) as i32;
            let d2 = self.distance(
                previous,
                self.next_index(list, current),
                v,
                depth.wrapping_add(1),
            ) as i32;
            return d1.max(d2) as u32;
        }
        let prev = self.location(self.order(list, previous).unwrap(), v, true);
        let cur = self.location(order, v, true);
        if prev == u32::MAX || cur == u32::MAX {
            return 0;
        }
        self.query(
            32,
            v,
            u64::from(prev),
            u64::from(cur),
            u64::from(self.query(4, v, 0, 0, 0) == 3),
        ) as u32
    }
    fn valid_orders(self, v: *mut c_void) -> bool {
        let list = self.orders(v);
        !list.is_null()
            && (0..self.stored_count(list))
                .any(|i| matches!(self.stored_order(list, i).kind(), 1 | 2 | 6))
    }
}
impl Control {
    async fn destination(
        &self,
        v: *mut c_void,
        mut order: Order,
        mut depth: i32,
        pbs: bool,
    ) -> bool {
        let g = self.g;
        loop {
            if depth > i32::from(g.count(v)) {
                let mut current = g.current(v);
                current.kind = 0;
                current.flags = 0;
                current.destination = 0;
                g.put_current(v, current);
                self.reentry(6, v, 0, 0, 0).await;
                return false;
            }
            match order.kind() {
                1 => {
                    let tile = g.query(33, v, u64::from(order.destination), 0, 0);
                    self.reentry(6, v, tile, 0, 0).await;
                    return true;
                }
                2 => {
                    if order.depot_type() & 1 != 0 && g.query(26, v, 0, 0, 0) == 0 {
                        g.update_timetable(v, true);
                        g.increment_real(v);
                    } else if g.current(v).action() & 2 != 0 {
                        if g.query(34, v, 0, 0, 0) == 0
                            && g.query(13, ptr::null_mut(), 0, 0, 0) != g.query(20, v, 0, 0, 0) % 74
                        {
                        } else {
                            let mut closest = Closest::default();
                            self.reentry(7, v, ptr::addr_of_mut!(closest) as usize as u64, 0, 0)
                                .await;
                            if closest.found != 0 {
                                if pbs && closest.reverse != 0 {
                                    return false;
                                }
                                self.reentry(6, v, u64::from(closest.tile), 0, 0).await;
                                let mut current = g.current(v);
                                current.destination = closest.destination;
                                g.put_current(v, current);
                                if g.query(4, v, 0, 0, 0) == 0 && closest.reverse != 0 {
                                    self.reentry(4, v, 0, 0, 0).await;
                                }
                                if g.query(4, v, 0, 0, 0) == 3
                                    && g.query(35, v, 0, 0, 0) != 0
                                    && g.query(36, v, 0, 0, 0) != u64::from(closest.destination)
                                {
                                    self.reentry(5, v, 0, 0, 0).await;
                                }
                                return true;
                            }
                            if pbs {
                                return false;
                            }
                            g.update_timetable(v, true);
                            g.increment_real(v);
                        }
                    } else {
                        if g.query(4, v, 0, 0, 0) != 3 {
                            let tile =
                                g.query(31, ptr::null_mut(), u64::from(order.destination), 0, 0);
                            self.reentry(6, v, tile, 0, 0).await;
                        } else {
                            let destination = g.current(v).destination;
                            if g.query(36, v, 0, 0, 0) != u64::from(destination) {
                                let tile = g.query(33, v, u64::from(destination), 0, 0);
                                self.reentry(6, v, tile, 0, 0).await;
                            }
                        }
                        return true;
                    }
                }
                6 => {
                    let tile = g.query(37, ptr::null_mut(), u64::from(order.destination), 0, 0);
                    self.reentry(6, v, tile, 0, 0).await;
                    return true;
                }
                7 => {
                    let next = g.conditional(order, v);
                    if next != u8::MAX {
                        g.update_timetable(v, false);
                        let state = g.consist(v);
                        unsafe {
                            (*state).implicit = next;
                            (*state).real = next;
                        }
                        g.update_real(v);
                        let travel = g
                            .vehicle_order(v, unsafe { (*state).real })
                            .unwrap()
                            .timetabled_travel();
                        unsafe {
                            (*state).time = (*state).time.wrapping_add(i32::from(travel));
                        }
                        if g.query(4, v, 0, 0, 0) < 2 {
                            g.write(8, v, 1, 0, 0);
                        }
                    } else {
                        g.update_timetable(v, true);
                        g.increment_real(v);
                    }
                }
                _ => {
                    self.reentry(6, v, 0, 0, 0).await;
                    return false;
                }
            }
            let real = unsafe { (*g.consist(v)).real };
            let next = g.vehicle_order(v, real).filter(|o| o.kind() != 8);
            let Some(next) = next else {
                let mut current = g.current(v);
                current.kind = 0;
                current.flags = 0;
                current.destination = 0;
                g.put_current(v, current);
                self.reentry(6, v, 0, 0, 0).await;
                return false;
            };
            g.put_current(v, next);
            order = next;
            depth = depth.wrapping_add(1);
        }
    }
    async fn process(&self, v: *mut c_void) -> bool {
        let g = self.g;
        let current = g.current(v);
        let kind = g.query(4, v, 0, 0, 0);
        match current.kind() {
            2 => {
                if current.depot_type() & 2 == 0 {
                    return false;
                }
            }
            3 => return false,
            4 => {
                if kind != 3 {
                    return false;
                }
            }
            _ => {}
        }
        let reverse = current.kind() == 0;
        if ((current.kind() == 1 && current.non_stop() & 2 != 0) || current.kind() == 6)
            && g.query(38, v, 0, 0, 0) != 0
            && u64::from(current.destination) == g.query(39, v, 0, 0, 0)
        {
            self.delete_implicit(v);
            g.write(12, v, u64::from(current.destination), 0, 0);
            g.update_timetable(v, true);
            g.increment_implicit(v);
        }
        g.update_real(v);
        let order = g
            .vehicle_order(v, unsafe { (*g.consist(v)).real })
            .filter(|o| o.kind() != 8);
        if order.is_none() || (kind == 3 && !g.valid_orders(v)) {
            if kind == 3 {
                self.reentry(16, v, 0, 0, 0).await;
                return false;
            }
            let mut current = g.current(v);
            current.kind = 0;
            current.flags = 0;
            current.destination = 0;
            g.put_current(v, current);
            self.reentry(6, v, 0, 0, 0).await;
            return false;
        }
        let order = order.unwrap();
        if order.equals(g.current(v))
            && (kind == 3 || g.query(34, v, 0, 0, 0) != 0)
            && (kind != 2
                || order.kind() != 1
                || g.query(40, ptr::null_mut(), u64::from(order.destination), 0, 0)
                    != u64::from(u32::MAX))
        {
            return false;
        }
        g.put_current(v, order);
        g.write(5, v, (-2i64) as u64, 0, 0);
        if kind == 2 || kind == 3 {
            g.write(13, v, 0, 0, 0);
        }
        self.destination(v, order, 0, false).await && reverse
    }
}

impl Game {
    fn error_with(self, output: *mut c_void, error: u32, detail: u64) -> u64 {
        self.write(3, output, u64::from(error), detail, 0);
        0
    }
    fn check_owner(self, owner: u64, output: *mut c_void) -> bool {
        self.query(44, ptr::null_mut(), owner, output as usize as u64, 0) != 0
    }
    fn error(self, output: *mut c_void, error: u32) -> u64 {
        u64::from(self.command_error(output, error))
    }
}
impl Control {
    fn insert_command(
        &self,
        v: *mut c_void,
        index: u8,
        order: Order,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if !g.primary(v) {
            return g.error(output, 0);
        }
        if !g.owner_check(v, output) {
            return 0;
        }
        if order.refit != 0xfe || order.wait != 0 || order.travel != 0 || order.speed != u16::MAX {
            return g.error(output, 0);
        }
        let kind = g.query(4, v, 0, 0, 0);
        let destination = u64::from(order.destination);
        match order.kind() {
            1 => {
                if g.query(41, ptr::null_mut(), destination, 0, 0) == 0 {
                    return g.error(output, 0);
                }
                let owner = g.query(42, ptr::null_mut(), destination, 0, 0);
                if owner != 0x10 && !g.check_owner(owner, output) {
                    return 0;
                }
                if g.query(43, v, destination, 0, 0) == 0 {
                    return g.error_with(output, 5, g.query(45, v, destination, 0, 0));
                }
                let mut u = g.first(v);
                while !u.is_null() {
                    if g.query(43, u, destination, 0, 0) == 0 {
                        return g.error_with(output, 6, g.query(45, u, destination, 0, 0));
                    }
                    u = g.next(u);
                }
                if order.non_stop() != 0 && kind >= 2 {
                    return g.error(output, 0);
                }
                match order.load() {
                    0 | 4 => {}
                    2 | 3 => {
                        if g.has(v, 3) {
                            return g.error(output, 7);
                        }
                    }
                    _ => return g.error(output, 0),
                }
                if !matches!(order.unload(), 0 | 1 | 2 | 4) {
                    return g.error(output, 0);
                }
                match (order.kind >> 4) & 3 {
                    0 | 1 => {
                        if kind != 0 {
                            return g.error(output, 0);
                        }
                    }
                    2 => {}
                    _ => return g.error(output, 0),
                }
            }
            2 => {
                if order.action() & 2 == 0 {
                    if kind == 3 {
                        if g.query(41, ptr::null_mut(), destination, 0, 0) == 0 {
                            return g.error(output, 0);
                        }
                        if !g.check_owner(g.query(42, ptr::null_mut(), destination, 0, 0), output) {
                            return 0;
                        }
                        if g.query(43, v, destination, 0, 0) == 0
                            || g.query(47, ptr::null_mut(), destination, 0, 0) == 0
                        {
                            return g.error(output, 0);
                        }
                    } else {
                        if g.query(48, ptr::null_mut(), destination, 0, 0) == 0 {
                            return g.error(output, 0);
                        }
                        if !g.check_owner(g.query(49, ptr::null_mut(), destination, 0, 0), output) {
                            return 0;
                        }
                        if kind > 2
                            || g.query(50 + kind as u32, ptr::null_mut(), destination, 0, 0) == 0
                        {
                            return g.error(output, 0);
                        }
                    }
                }
                if order.non_stop() != 0 && kind >= 2 {
                    return g.error(output, 0);
                }
                let mut flags = order.depot_type();
                if flags & 2 != 0 {
                    flags &= !1;
                }
                flags &= !2;
                if flags != 0 || order.action() & !7 != 0 {
                    return g.error(output, 0);
                }
                if order.depot_type() & 1 != 0 && order.action() & 5 != 0 {
                    return g.error(output, 0);
                }
                if order.action() & 4 != 0 {
                    if g.has(v, 1) {
                        return g.error(output, 8);
                    }
                    if g.has(v, 3) {
                        return g.error(output, 9);
                    }
                    if g.has(v, 2) {
                        return g.error(output, 10);
                    }
                }
            }
            6 => {
                if g.query(53, ptr::null_mut(), destination, 0, 0) == 0 {
                    return g.error(output, 0);
                }
                let facilities = g.query(54, ptr::null_mut(), destination, 0, 0);
                match kind {
                    0 => {
                        if facilities & 1 == 0 {
                            return g.error(output, 11);
                        }
                        if !g.check_owner(g.query(55, ptr::null_mut(), destination, 0, 0), output) {
                            return 0;
                        }
                    }
                    1 => {
                        if facilities & 6 == 0 {
                            return g.error(output, 12);
                        }
                        if !g.check_owner(g.query(55, ptr::null_mut(), destination, 0, 0), output) {
                            return 0;
                        }
                    }
                    2 => {
                        if facilities & 16 == 0 {
                            return g.error(output, 13);
                        }
                        let owner = g.query(55, ptr::null_mut(), destination, 0, 0);
                        if owner != 0x10 && !g.check_owner(owner, output) {
                            return 0;
                        }
                    }
                    _ => return g.error(output, 0),
                }
                if order.non_stop() != 0 && kind >= 2 {
                    return g.error(output, 0);
                }
            }
            7 => {
                if order.flags != 0 && order.flags >= g.count(v) {
                    return g.error(output, 0);
                }
                if order.variable() >= 8 {
                    return g.error(output, 0);
                }
                if g.has(v, 3) {
                    return g.error(output, 14);
                }
                let compare = order.comparator();
                if compare >= 8 {
                    return g.error(output, 0);
                }
                match order.variable() {
                    4 => {
                        if compare != 6 && compare != 7 {
                            return g.error(output, 0);
                        }
                    }
                    5 => {
                        if compare != 0 || order.value() != 0 {
                            return g.error(output, 0);
                        }
                    }
                    variable => {
                        if matches!(variable, 0 | 1 | 7) && order.value() > 100 {
                            return g.error(output, 0);
                        }
                        if compare == 6 || compare == 7 {
                            return g.error(output, 0);
                        }
                    }
                }
            }
            _ => return g.error(output, 0),
        }
        if index > g.count(v) {
            return g.error(output, 0);
        }
        if g.count(v) >= 254 {
            return g.error(output, 15);
        }
        if g.orders(v).is_null() && g.query(56, ptr::null_mut(), 0, 0, 0) == 0 {
            return g.error(output, 16);
        }
        if execute {
            self.insert(v, order, index);
        }
        1
    }
    async fn declone(&self, v: *mut c_void, execute: bool) -> u64 {
        if execute {
            self.delete_vehicle_orders(v, false, true).await;
            self.g.write(5, v, (-1i64) as u64, 0, 0);
            self.g.write(9, v, 0, 0, 0);
        }
        1
    }
    async fn delete_command(
        &self,
        v: *mut c_void,
        index: u8,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if !g.primary(v) {
            return g.error(output, 0);
        }
        if !g.owner_check(v, output) {
            return 0;
        }
        if index >= g.count(v) {
            return self.declone(v, execute).await;
        }
        if g.vehicle_order(v, index).is_none() {
            return g.error(output, 0);
        }
        if execute {
            self.delete(v, index);
        }
        1
    }
    async fn skip_command(
        &self,
        v: *mut c_void,
        index: u8,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if !g.primary(v) {
            return g.error(output, 0);
        }
        let state = g.consist(v);
        if index == unsafe { (*state).implicit } || index >= g.count(v) || g.count(v) < 2 {
            return g.error(output, 0);
        }
        if !g.owner_check(v, output) {
            return 0;
        }
        if execute {
            if g.current(v).kind() == 3 {
                self.reentry(3, v, 0, 0, 0).await;
            }
            unsafe {
                (*state).implicit = index;
                (*state).real = index;
            }
            g.update_real(v);
            g.reset(v);
            g.write(5, v, (-2i64) as u64, 0, 0);
            if matches!(g.query(4, v, 0, 0, 0), 2 | 3) {
                g.write(13, v, 0, 0, 0);
            }
        }
        1
    }
    fn move_command(
        &self,
        v: *mut c_void,
        from: u8,
        to: u8,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if !g.primary(v) {
            return g.error(output, 0);
        }
        if !g.owner_check(v, output) {
            return 0;
        }
        if from >= g.count(v)
            || to >= g.count(v)
            || from == to
            || g.count(v) <= 1
            || g.vehicle_order(v, from).is_none()
        {
            return g.error(output, 0);
        }
        if execute {
            self.move_order(v, from, to);
        }
        1
    }
    fn modify_command(
        &self,
        v: *mut c_void,
        index: u8,
        field: u8,
        data: u16,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if field >= 9 {
            return g.error(output, 0);
        }
        if !g.primary(v) {
            return g.error(output, 0);
        }
        if !g.owner_check(v, output) {
            return 0;
        }
        if index >= g.count(v) {
            return g.error(output, 0);
        }
        let list = g.orders(v);
        let mut order = g.order(list, index).unwrap();
        match order.kind() {
            1 => {
                if !matches!(field, 0..=3) {
                    return g.error(output, 0);
                }
            }
            2 => {
                if !matches!(field, 0 | 4) {
                    return g.error(output, 0);
                }
            }
            6 => {
                if field != 0 {
                    return g.error(output, 0);
                }
            }
            7 => {
                if !matches!(field, 5..=8) {
                    return g.error(output, 0);
                }
            }
            _ => return g.error(output, 0),
        }
        let byte = data as u8;
        match field {
            0 => {
                if g.query(4, v, 0, 0, 0) >= 2 || byte == order.non_stop() || byte & !3 != 0 {
                    return g.error(output, 0);
                }
            }
            1 => {
                if g.query(4, v, 0, 0, 0) != 0 || data >= 3 {
                    return g.error(output, 0);
                }
            }
            2 => {
                if order.non_stop() & 2 != 0
                    || byte == order.unload()
                    || !matches!(byte, 0 | 1 | 2 | 4)
                {
                    return g.error(output, 0);
                }
            }
            3 => {
                if order.non_stop() & 2 != 0 || byte == order.load() {
                    return g.error(output, 0);
                }
                match byte {
                    0 | 4 => {}
                    2 | 3 => {
                        if g.has(v, 3) {
                            return g.error(output, 7);
                        }
                    }
                    _ => return g.error(output, 0),
                }
            }
            4 => {
                if byte >= 4 {
                    return g.error(output, 0);
                }
                if byte == 3 {
                    if g.has(v, 3) && order.action() & 4 == 0 {
                        return g.error(output, 9);
                    }
                    if g.has(v, 2) {
                        return g.error(output, 10);
                    }
                    if g.has(v, 1) {
                        return g.error(output, 8);
                    }
                }
            }
            5 => {
                if byte >= 8 {
                    return g.error(output, 0);
                }
            }
            6 => {
                if byte >= 8 {
                    return g.error(output, 0);
                }
                match order.variable() {
                    5 => return g.error(output, 0),
                    4 => {
                        if byte != 6 && byte != 7 {
                            return g.error(output, 0);
                        }
                    }
                    _ => {
                        if byte == 6 || byte == 7 {
                            return g.error(output, 0);
                        }
                    }
                }
            }
            7 => match order.variable() {
                4 | 5 => return g.error(output, 0),
                0 | 1 | 7 => {
                    if data > 100 {
                        return g.error(output, 0);
                    }
                }
                _ => {
                    if data > 2047 {
                        return g.error(output, 0);
                    }
                }
            },
            8 => {
                if data >= u16::from(g.count(v)) {
                    return g.error(output, 0);
                }
            }
            _ => unreachable!(),
        }
        if execute {
            match field {
                0 => {
                    order.kind = (order.kind & !0xc0) | ((byte & 3) << 6);
                    if order.non_stop() & 2 != 0 {
                        order.refit = 0xfe;
                        order.flags &= !0x77;
                    }
                }
                1 => order.kind = (order.kind & !0x30) | ((byte & 3) << 4),
                2 => order.flags = (order.flags & !7) | (byte & 7),
                3 => {
                    order.flags = (order.flags & !0x70) | ((byte & 7) << 4);
                    if order.load() == 4 {
                        order.refit = 0xfe;
                    }
                }
                4 => match byte {
                    0 => {
                        order.flags &= !(1 | 8 | 32);
                    }
                    1 => {
                        order.flags = (order.flags | 1) & !(8 | 32);
                        order.refit = 0xfe;
                    }
                    2 => {
                        order.flags = (order.flags | 8) & !(1 | 32);
                        order.refit = 0xfe;
                    }
                    3 => {
                        order.flags = (order.flags | 32) & !(1 | 8);
                    }
                    _ => unreachable!(),
                },
                5 => {
                    order.destination = (order.destination & 2047) | (u16::from(byte & 31) << 11);
                    let compare = order.comparator();
                    match order.variable() {
                        5 => {
                            order.kind &= !0xe0;
                            order.destination &= !2047;
                        }
                        4 => {
                            if compare != 6 && compare != 7 {
                                order.kind = (order.kind & !0xe0) | (6 << 5);
                            }
                            order.destination &= !2047;
                        }
                        variable => {
                            if matches!(variable, 0 | 1 | 7) && order.value() > 100 {
                                order.destination = (order.destination & !2047) | 0x64;
                            }
                            if compare == 6 || compare == 7 {
                                order.kind &= !0xe0;
                            }
                        }
                    }
                }
                6 => order.kind = (order.kind & !0xe0) | ((byte & 7) << 5),
                7 => order.destination = (order.destination & !2047) | (data & 2047),
                8 => order.flags = byte,
                _ => unreachable!(),
            }
            g.put_order(list, index, order);
            let mut u = g.first(v);
            g.write(7, u, 0, 0, 0);
            while !u.is_null() {
                let state = g.consist(u);
                let mut current = g.current(u);
                if index == unsafe { (*state).real }
                    && matches!(current.kind(), 1 | 3)
                    && current.load() != order.load()
                {
                    current.flags = (current.flags & !0x70) | ((order.load() & 7) << 4);
                    g.put_current(u, current);
                }
                g.reset(u);
                g.write(5, u, (-2i64) as u64, 0, 0);
                u = g.next(u);
            }
        }
        1
    }
    fn refit_command(
        &self,
        v: *mut c_void,
        index: u8,
        cargo: u8,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if cargo >= 64 && cargo != 0xfe && cargo != 0xfd {
            return g.error(output, 0);
        }
        if !g.primary(v) {
            return g.error(output, 0);
        }
        if !g.owner_check(v, output) {
            return 0;
        }
        let Some(mut order) = g.vehicle_order(v, index) else {
            return g.error(output, 0);
        };
        if cargo == 0xfd && order.kind() != 1 {
            return g.error(output, 0);
        }
        if order.load() == 4 {
            return g.error(output, 0);
        }
        if execute {
            order.refit = cargo;
            if cargo != 0xfe && order.kind() == 2 {
                order.flags &= !(1 | 8);
            }
            g.put_order(g.orders(v), index, order);
            let mut u = g.first(v);
            while !u.is_null() {
                g.write(5, u, (-2i64) as u64, 0, 0);
                if index == unsafe { (*g.consist(u)).real } && g.current(u).depot_type() & 2 != 0 {
                    let mut current = g.current(u);
                    current.refit = cargo;
                    g.put_current(u, current);
                }
                u = g.next(u);
            }
        }
        1
    }
}

impl Vector {
    unsafe fn transfer(&mut self, other: *mut Self) {
        // Both owners are live/disjoint and no typed order pointer is used during
        // transfer. Existing C++ Order lifetimes move with the allocation.
        unsafe {
            std::mem::swap(&mut self.storage, &mut (*other).storage);
            std::mem::swap(&mut self.length, &mut (*other).length);
        }
    }
}
impl Game {
    fn backup(self, b: *mut c_void) -> *mut Backup {
        unsafe { (self.0.backup)(b) }
    }
    fn backup_vector(self, b: *mut c_void) -> *mut Vector {
        unsafe { (self.0.backup_vector)(b) }
    }
    fn backup_consist(self, b: *mut c_void) -> *mut Consist {
        unsafe { (self.0.backup_consist)(b) }
    }
    fn next_backup(self, first: u32) -> *mut c_void {
        self.query(57, ptr::null_mut(), u64::from(first), 0, 0) as usize as *mut c_void
    }
    fn next_vehicle(self, first: u32) -> *mut c_void {
        self.query(58, ptr::null_mut(), u64::from(first), 0, 0) as usize as *mut c_void
    }
    fn backed_order(self, b: *mut c_void, i: usize) -> Order {
        unsafe { (*self.backup_vector(b)).data().add(i).read() }
    }
    fn captured(self, backup: *mut c_void, v: *mut c_void, user: u32) {
        let state = self.backup(backup);
        unsafe {
            (*state).user = user;
            (*state).tile = self.query(69, v, 0, 0, 0) as u32;
            (*state).group = self.query(70, v, 0, 0, 0) as u16;
            openttd_rust_consist_copy(self.backup_consist(backup), self.consist(v));
        }
        self.write(14, backup, v as usize as u64, 0, 0);
        let list = self.orders(v);
        if !list.is_null() && unsafe { (*self.list(list)).vehicles > 1 } {
            let first = self.first(v);
            let clone = if first == v { self.next(v) } else { first };
            unsafe {
                (*state).clone = clone;
            }
        } else if !list.is_null() {
            let vector = self.vector(list);
            let data = unsafe { (*vector).data() };
            let length = unsafe { (*vector).length };
            unsafe {
                openttd_rust_order_vector_assign(self.backup_vector(backup), data, length);
            }
        }
    }
    fn aircraft_range(self, dst: *mut c_void, src: *mut c_void) -> bool {
        if self.query(59, dst, 0, 0, 0) == 0 || self.count(src) == 0 {
            return true;
        }
        let list = self.orders(src);
        let max = self.query(60, dst, 0, 0, 0) as u32;
        for i in 0..self.size(list) {
            if matches!(self.order(list, i).unwrap().kind(), 1 | 2 | 6)
                && self.distance(i, self.next_index(list, i), src, 0) > max
            {
                return false;
            }
        }
        true
    }
}
impl Control {
    async fn clone_command(
        &self,
        dst: *mut c_void,
        src: *mut c_void,
        action: u8,
        execute: bool,
        output: *mut c_void,
    ) -> u64 {
        let g = self.g;
        if !g.primary(dst) {
            return g.error(output, 0);
        }
        if !g.owner_check(dst, output) {
            return 0;
        }
        if action == 2 {
            return self.declone(dst, execute).await;
        }
        if action != 0 && action != 1 {
            return g.error(output, 0);
        }
        if !g.primary(src) || g.query(4, dst, 0, 0, 0) != g.query(4, src, 0, 0, 0) || dst == src {
            return g.error(output, 0);
        }
        if !g.owner_check(src, output) {
            return 0;
        }
        let kind = g.query(4, src, 0, 0, 0);
        if action == 0 {
            if kind == 1 && g.query(61, src, 0, 0, 0) != g.query(61, dst, 0, 0, 0) {
                return g.error(output, 0);
            }
            if g.first(src) == g.first(dst) {
                return g.error(output, 0);
            }
        }
        let list = g.orders(src);
        if !list.is_null() {
            for i in 0..g.stored_count(list) {
                let order = g.stored_order(list, i);
                if order.kind() != 1
                    && !(kind == 3 && order.kind() == 2 && order.destination != u16::MAX)
                {
                    continue;
                }
                let destination = u64::from(order.destination);
                if (action == 1 || g.query(43, src, destination, 0, 0) != 0)
                    && g.query(43, dst, destination, 0, 0) == 0
                {
                    return g.error_with(output, 17, g.query(45, dst, destination, 0, 0));
                }
            }
        }
        if kind == 3 && !g.aircraft_range(dst, src) {
            return g.error(output, 18);
        }
        if (action == 1 || list.is_null()) && g.query(56, ptr::null_mut(), 0, 0, 0) == 0 {
            return g.error(output, 16);
        }
        if execute {
            self.delete_vehicle_orders(dst, action == 1, g.count(dst) != g.count(src))
                .await;
            if action == 0 {
                unsafe {
                    (*g.vehicle(dst)).orders = g.orders(src);
                }
                self.add_shared(dst, src);
                g.write(5, dst, (-1i64) as u64, 0, 0);
                g.write(5, src, (-2i64) as u64, 0, 0);
                g.write(9, dst, 0, 0, 0);
            } else {
                // Original copies the source before deleting the kept empty shell.
                // Rust temporary storage is a scalar copy, never a live mirror.
                let mut copied = Vec::new();
                let source = g.orders(src);
                if !source.is_null() {
                    let vector = g.vector(source);
                    for i in 0..unsafe { (*vector).length } {
                        copied.push(unsafe { (*vector).data().add(i).read() });
                    }
                }
                let old = g.orders(dst);
                if !old.is_null() {
                    self.reentry(1, old, 0, 0, 0).await;
                }
                let list = self.new_list(dst, None, ptr::null_mut());
                unsafe {
                    openttd_rust_order_vector_assign(g.vector(list), copied.as_ptr(), copied.len());
                }
                // Initialize after copied orders, retaining original total calculation.
                self.initialize_list(list, dst);
                unsafe {
                    (*g.vehicle(dst)).orders = list;
                }
                g.write(5, dst, (-1i64) as u64, 0, 0);
                g.write(9, dst, 0, 0, 0);
            }
        }
        1
    }
    async fn restore_backup(&self, backup: *mut c_void, v: *mut c_void) {
        let g = self.g;
        let state = g.backup(backup);
        let clone = unsafe { (*state).clone };
        if !clone.is_null() {
            self.reentry(9, v, clone as usize as u64, 0, 0).await;
        } else if unsafe { (*g.backup_vector(backup)).length > 0 }
            && g.query(56, ptr::null_mut(), 0, 0, 0) != 0
        {
            let list = g.query(17, ptr::null_mut(), 0, 0, 0) as usize as *mut c_void;
            unsafe {
                (*g.vector(list)).transfer(g.backup_vector(backup));
            }
            self.initialize_list(list, v);
            unsafe {
                (*g.vehicle(v)).orders = list;
            }
            g.write(2, ptr::null_mut(), 0, 0, 0);
        }
        if g.query(71, backup, 0, 0, 0) == 0 {
            g.write(15, backup, 0, 0, 0);
        }
        unsafe {
            openttd_rust_consist_copy(g.consist(v), g.backup_consist(backup));
        }
        g.write(16, v, backup as usize as u64, 0, 0);
        g.update_real(v);
        let vs = g.consist(v);
        if unsafe { (*vs).implicit >= g.count(v) } {
            unsafe {
                (*vs).implicit = (*vs).real;
            }
        }
        self.reentry(10, v, u64::from(unsafe { (*state).group }), 0, 0)
            .await;
    }
    async fn backup_operation(&self, kind: u32, v: *mut c_void, a: u64, b: u64, c: u64) -> u64 {
        let g = self.g;
        if kind == 0 {
            g.captured(v, a as usize as *mut c_void, b as u32);
            return 0;
        }
        if kind == 1 {
            let mut ob = g.next_backup(0);
            while !ob.is_null() {
                let next = (g.query(72, ob, 0, 0, 0) as u32).wrapping_add(1);
                if unsafe { (*g.backup(ob)).user == a as u32 } {
                    self.reentry(11, ob, 0, 0, 0).await;
                }
                ob = g.next_backup(next);
            }
            if g.query(62, ptr::null_mut(), 0, 0, 0) != 0 {
                g.query(63, v, a, 0, 0);
            }
            return 0;
        }
        if kind == 2 {
            let mut ob = g.next_backup(0);
            while !ob.is_null() {
                let next = (g.query(72, ob, 0, 0, 0) as u32).wrapping_add(1);
                let state = g.backup(ob);
                if g.query(69, v, 0, 0, 0) as u32 == unsafe { (*state).tile }
                    && unsafe { (*state).user == a as u32 }
                {
                    self.restore_backup(ob, v).await;
                    self.reentry(11, ob, 0, 0, 0).await;
                }
                ob = g.next_backup(next);
            }
            return 0;
        }
        if kind == 4 {
            let mut ob = g.next_backup(0);
            while !ob.is_null() {
                if unsafe { (*g.backup(ob)).user == a as u32 } {
                    self.reentry(14, ptr::null_mut(), a, 0, 0).await;
                    return 0;
                }
                ob = g.next_backup((g.query(72, ob, 0, 0, 0) as u32).wrapping_add(1));
            }
            return 0;
        }
        let user = if kind == 5
            && g.query(64, ptr::null_mut(), 0, 0, 0) != 0
            && g.query(65, ptr::null_mut(), 0, 0, 0) == 0
        {
            g.query(66, ptr::null_mut(), 0, 0, 0) as u32
        } else {
            g.query(67, ptr::null_mut(), 0, 0, 0) as u32
        };
        let mut ob = g.next_backup(0);
        while !ob.is_null() {
            let next = (g.query(72, ob, 0, 0, 0) as u32).wrapping_add(1);
            let state = g.backup(ob);
            match kind {
                3 => {
                    if unsafe {
                        (*state).user == b as u32
                            && ((*state).tile == a as u32 || a as u32 == u32::MAX)
                    } {
                        self.reentry(11, ob, 0, 0, 0).await;
                    }
                }
                5 => {
                    if (b == 0 || unsafe { (*state).user == user })
                        && (a as u32 == u32::MAX || a as u32 == unsafe { (*state).tile })
                    {
                        if b != 0 {
                            self.reentry(
                                13,
                                ptr::null_mut(),
                                u64::from(unsafe { (*state).tile }),
                                u64::from(user),
                                0,
                            )
                            .await;
                        } else {
                            self.reentry(11, ob, 0, 0, 0).await;
                        }
                    }
                }
                6 => {
                    if unsafe { (*state).group == a as u16 } {
                        unsafe {
                            (*state).group = g.query(68, ptr::null_mut(), 0, 0, 0) as u16;
                        }
                    }
                }
                7 => {
                    if unsafe { (*state).clone == v } {
                        let first = g.first(v);
                        let clone = if first == v { g.next(v) } else { first };
                        unsafe {
                            (*state).clone = clone;
                        }
                        if clone.is_null() {
                            self.reentry(11, ob, 0, 0, 0).await;
                        }
                    }
                }
                8 => {
                    let length = unsafe { (*g.backup_vector(ob)).length };
                    for i in 0..length {
                        let order = g.backed_order(ob, i);
                        let mut kind = order.kind();
                        if kind == 2 && order.action() & 2 != 0 {
                            continue;
                        }
                        let hangar = g.query(73, ob, 0, 0, 0) != 0;
                        if kind == 2 && c != 0 && !hangar {
                            continue;
                        }
                        if kind == 8 || (hangar && kind == 2 && c == 0) {
                            kind = 1;
                        }
                        if u64::from(kind) == a && u64::from(order.destination) == b {
                            self.reentry(11, ob, 0, 0, 0).await;
                            break;
                        }
                    }
                }
                _ => unreachable!(),
            }
            ob = g.next_backup(next);
        }
        0
    }
}

impl Control {
    async fn remove_destination(&self, kind: u8, destination: u16, hangar: bool) {
        let g = self.g;
        let mut v = g.next_vehicle(0);
        while !v.is_null() {
            let next = (g.query(20, v, 0, 0, 0) as u32).wrapping_add(1);
            let aircraft = g.query(4, v, 0, 0, 0) == 3;
            let mut current = g.current(v);
            let current_kind = if aircraft && current.kind() == 2 && !hangar {
                1
            } else {
                current.kind()
            };
            if current_kind == kind && (!hangar || aircraft) && current.destination == destination {
                current.kind = 5;
                current.flags = 0;
                g.put_current(v, current);
                g.write(6, v, 0, 0, 0);
            }
            if !g.orders(v).is_null() {
                let mut index = 0u8;
                while index < g.count(v) {
                    let mut next = index.wrapping_add(1);
                    let list = g.orders(v);
                    let mut order = g.order(list, index).unwrap();
                    let mut ty = order.kind();
                    if !(ty == 2 && (order.action() & 2 != 0 || (hangar && !aircraft))) {
                        if ty == 8 || (aircraft && ty == 2 && !hangar) {
                            ty = 1;
                        }
                        if ty == kind && order.destination == destination {
                            if order.kind() == 8 {
                                self.delete(v, index);
                                next = index;
                            } else {
                                let state = g.list(list);
                                unsafe {
                                    (*state).total =
                                        (*state).total.wrapping_sub(i32::from(order.wait));
                                }
                                if order.wait_timetabled() {
                                    unsafe {
                                        (*state).timetable = (*state)
                                            .timetable
                                            .wrapping_sub(i32::from(order.timetabled_wait()));
                                    }
                                    order.set_wait_timetabled(false);
                                }
                                order.wait = 0;
                                let travel = order.travel_timetabled();
                                order.kind = 5;
                                order.flags = 0;
                                order.set_travel_timetabled(travel);
                                g.put_order(list, index, order);
                                let mut shared = g.first(v);
                                while !shared.is_null() {
                                    let data = u16::from(index) | (u16::from(u8::MAX) << 8);
                                    g.write(5, shared, u64::from(data), 0, 0);
                                    g.write(5, shared, u64::from(data), 0, 0);
                                    shared = g.next(shared);
                                }
                            }
                        }
                    }
                    index = next;
                }
            }
            v = g.next_vehicle(next);
        }
        self.backup_operation(
            8,
            ptr::null_mut(),
            u64::from(kind),
            u64::from(destination),
            u64::from(hangar),
        )
        .await;
    }
    fn check_orders(&self, v: *mut c_void) {
        let g = self.g;
        let review = g.query(74, ptr::null_mut(), 0, 0, 0);
        if review == 0
            || g.query(5, v, 0, 0, 0) & 128 != 0
            || (review == 1 && g.query(5, v, 0, 0, 0) & 2 != 0)
            || g.first(v) != v
        {
            return;
        }
        if g.query(75, v, 0, 0, 0) == 0 || !g.query(76, v, 0, 0, 0).is_multiple_of(20) {
            return;
        }
        let mut message = 0u32;
        let mut stations = 0i32;
        let list = g.orders(v);
        if !list.is_null() {
            for i in 0..g.stored_count(list) {
                let order = g.stored_order(list, i);
                if order.kind() == 5 {
                    message = 1;
                    break;
                }
                if order.kind() == 1 {
                    stations = stations.wrapping_add(1);
                    if g.query(43, v, u64::from(order.destination), 0, 0) == 0 {
                        message = 2;
                    } else if g.query(4, v, 0, 0, 0) == 3
                        && g.query(78, v, 0, 0, 0) != 0
                        && g.query(79, ptr::null_mut(), u64::from(order.destination), 0, 0) != 0
                        && g.query(80, ptr::null_mut(), 0, 0, 0) == 0
                        && message == 0
                    {
                        message = 3;
                    }
                }
            }
        }
        if g.count(v) > 1
            && g.stored_order(list, 0)
                .equals(g.stored_order(list, g.stored_count(list) - 1))
        {
            message = 4;
        }
        if stations < 2 && message == 0 {
            message = 5;
        }
        g.write(18, v, 0, 0, 0);
        if message != 0 {
            g.write(17, v, u64::from(message), 0, 0);
        }
    }
}

impl Control {
    fn begin_loading_orders(&self, v: *mut c_void) {
        let g = self.g;
        let current = g.current(v);
        let last = g.query(2, v, 0, 0, 0) as u16;
        if current.kind() == 1 && current.destination == last {
            self.delete_implicit(v);
            let mut current = g.current(v);
            current.kind = 3;
            g.put_current(v, current);
            g.update_timetable(v, true);
            let mut current = g.current(v);
            current.kind = (current.kind & !0xc0) | 0xc0;
            g.put_current(v, current);
            return;
        }
        let state = g.consist(v);
        let implicit = unsafe { (*state).implicit };
        let in_list = g.vehicle_order(v, implicit);
        if g.query(4, v, 0, 0, 0) < 2
            && !in_list.is_some_and(|o| o.kind() == 8 && o.destination == last)
        {
            let suppress = g.query(18, v, 0, 0, 0) != 0;
            let previous = if implicit > 0 {
                g.vehicle_order(v, implicit - 1)
            } else if g.count(v) > 1 {
                g.vehicle_order(v, g.count(v) - 1)
            } else {
                None
            };
            if !previous.is_some_and(|o| matches!(o.kind(), 1 | 8) && o.destination == last) {
                let mut target = i32::from(implicit);
                let mut found = false;
                while target != i32::from(unsafe { (*state).real }) || g.manual(v) == 0 {
                    let Some(order) = g.vehicle_order(v, target as u8) else {
                        break;
                    };
                    if order.kind() == 8 && order.destination == last {
                        found = true;
                        break;
                    }
                    target = target.wrapping_add(1);
                    if target >= i32::from(g.count(v)) {
                        if g.manual(v) == 0 && g.count(v) < 32 {
                            break;
                        }
                        target = 0;
                    }
                    if target == i32::from(unsafe { (*state).implicit }) {
                        break;
                    }
                }
                if found {
                    if suppress {
                        unsafe {
                            (*state).implicit = target as u8;
                        }
                        g.write(5, v, 0, 0, 0);
                    } else {
                        loop {
                            let order = g.vehicle_order(v, unsafe { (*state).implicit }).unwrap();
                            if order.kind() == 8 && order.destination == last {
                                break;
                            }
                            if order.kind() == 8 {
                                self.delete(v, unsafe { (*state).implicit });
                            } else {
                                unsafe {
                                    (*state).implicit = (*state).implicit.wrapping_add(1);
                                }
                            }
                            if g.vehicle_order(v, unsafe { (*state).implicit }).is_none() {
                                unsafe {
                                    (*state).implicit = 0;
                                }
                            }
                        }
                    }
                } else if !suppress
                    && (if g.orders(v).is_null() {
                        g.query(56, ptr::null_mut(), 0, 0, 0) != 0
                    } else {
                        g.count(v) < 254
                    })
                {
                    let order = Order {
                        kind: 8,
                        flags: 0,
                        destination: last,
                        refit: 0xfe,
                        wait: 0,
                        travel: 0,
                        speed: u16::MAX,
                    };
                    self.insert(v, order, unsafe { (*state).implicit });
                    if unsafe { (*state).implicit > 0 } {
                        unsafe {
                            (*state).implicit = (*state).implicit.wrapping_sub(1);
                        }
                    }
                    g.write(8, v, 0, 0, 0);
                }
            }
        }
        let mut current = g.current(v);
        current.kind = 3;
        current.flags = 0;
        g.put_current(v, current);
    }
}
