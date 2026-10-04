/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! ScriptList-specific resumable VM control. Every host operation executes after return.
//! Controllers retain scalars only, never a list borrow, VM object or pending cleanup.
use super::List;
use std::ops::Bound::{Excluded, Unbounded};

const NULL: u32 = 0x0100_0001;
const INTEGER: u32 = 0x0500_0002;
const BOOL: u32 = 0x0100_0008;
const TABLE: u32 = 0x0a00_0020;
const ARRAY: u32 = 0x0800_0040;
const CLOSURE: u32 = 0x0800_0100;
const NATIVE: u32 = 0x0800_0200;

// These action IDs are the concrete ScriptList host protocol, not a generic VM API.
const RETURN: u32 = 0;
const TOP: u32 = 1;
const TYPE: u32 = 2;
const GET_INT: u32 = 3;
const GET_BOOL: u32 = 4;
const PUSH: u32 = 5;
const ROOT: u32 = 6;
const PUSH_INT: u32 = 7;
const PUSH_BOOL: u32 = 8;
const PUSH_NULL: u32 = 9;
const TAG: u32 = 10;
const ARRAY_NEW: u32 = 11;
const TABLE_NEW: u32 = 12;
const APPEND: u32 = 13;
const RAW_SET: u32 = 14;
const NEXT: u32 = 15;
const POP: u32 = 16;
const POP_TOP: u32 = 17;
const CALL: u32 = 18;
const CHARGE: u32 = 19;
const DISABLE: u32 = 20;
const LIMIT: u32 = 21;
const ERROR: u32 = 22;
const THROW_RESULT: u32 = 23;
const ITEM: u32 = 24;
const GET_PAIR: u32 = 25;
const LOG_NEXT: u32 = 26;
const LOG_END: u32 = 27;
const THROW_ERROR: u32 = 28;
const INDEX: u32 = 29;

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub(crate) struct Input {
    pub(crate) a: i64,
    pub(crate) b: i64,
    pub(crate) flag: u64,
    pub(crate) kind: u32,
}
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub(crate) struct Action {
    pub(crate) a: i64,
    pub(crate) kind: u32,
}
fn action(kind: u32, a: impl Into<i64>) -> Action {
    Action { kind, a: a.into() }
}

pub(crate) struct Control {
    operation: u8,
    phase: u8,
    nparam: i32,
    argument: i32,
    item: Option<i64>,
    value: i64,
    token: i32,
    bool_result: bool,
    sort: i32,
    ascending: bool,
}
impl Control {
    fn new(operation: u8) -> Self {
        assert!(operation <= 6);
        Self {
            operation,
            phase: 0,
            nparam: 0,
            argument: 0,
            item: None,
            value: 0,
            token: 0,
            bool_result: false,
            sort: 0,
            ascending: false,
        }
    }
    fn yield_at(&mut self, phase: u8, kind: u32, a: impl Into<i64>) -> Action {
        self.phase = phase;
        action(kind, a)
    }
    fn entry(&self, list: &List) -> Option<(i64, i64)> {
        let entry = if let Some(key) = self.item {
            list.items.range((Excluded(key), Unbounded)).next()
        } else {
            list.items.first_key_value()
        };
        entry.map(|(&key, &value)| (key, value))
    }
    // Original SQInteger-to-int narrowing (C++20 modulo conversion). Subsequent
    // signed int additions/subtractions must remain in the original defined domain.
    #[allow(clippy::cast_possible_truncation)]
    fn parameters(top: i64) -> i32 {
        top.checked_sub(1).expect("defined VM top") as i32
    }
    #[allow(clippy::cast_possible_truncation)]
    fn sort_kind(value: i64) -> i32 {
        value as i32
    }
    fn arguments(&mut self) -> Action {
        if self.argument < self.nparam - 1 {
            let index = self
                .argument
                .checked_add(3)
                .expect("defined argument index");
            self.argument += 1;
            action(PUSH, index)
        } else {
            self.yield_at(
                10,
                CALL,
                self.nparam.checked_add(1).expect("defined call count"),
            )
        }
    }
    #[allow(clippy::too_many_lines)] // Consecutive original callback phases stay visible together.
    fn callback(&mut self, list: &mut List, input: Input, filter: bool) -> Action {
        loop {
            match self.phase {
                0 => {
                    if !filter {
                        list.touch();
                    }
                    return self.yield_at(1, TOP, 0);
                }
                1 => {
                    self.nparam = Self::parameters(input.a);
                    if self.nparam < 1 {
                        if !filter {
                            return action(ERROR, 1);
                        }
                        return self.yield_at(4, DISABLE, 0);
                    }
                    return self.yield_at(2, TYPE, 2);
                }
                2 => {
                    if input.kind != CLOSURE && input.kind != NATIVE {
                        return action(if filter { THROW_ERROR } else { ERROR }, 2);
                    }
                    return if filter {
                        self.yield_at(3, PUSH, 2)
                    } else {
                        self.yield_at(3, DISABLE, 0)
                    };
                }
                3 => {
                    return if filter {
                        self.yield_at(4, DISABLE, 0)
                    } else {
                        self.yield_at(4, LIMIT, 0)
                    };
                }
                4 => {
                    return if filter {
                        if self.nparam >= 1 {
                            self.yield_at(5, LIMIT, 1)
                        } else {
                            self.yield_at(6, ITEM, 0)
                        }
                    } else {
                        self.yield_at(6, PUSH, 2)
                    };
                }
                5 => return self.yield_at(6, ITEM, 0),
                6 => {
                    if filter {
                        // Host input: 0=end, 1=invalid typed item, 2=valid typed item.
                        match input.flag {
                            0 => {
                                return if self.nparam >= 1 {
                                    self.yield_at(18, POP_TOP, 0)
                                } else {
                                    action(RETURN, 0)
                                };
                            }
                            1 => return action(ITEM, 0),
                            2 => return self.yield_at(21, INDEX, 0),
                            _ => std::process::abort(),
                        }
                    }
                    let Some((key, _)) = self.entry(list) else {
                        return self.yield_at(
                            18,
                            POP,
                            self.nparam.checked_add(3).expect("defined pop count"),
                        );
                    };
                    self.item = Some(key);
                    self.token = list.modifications;
                    return self.yield_at(7, ROOT, 0);
                }
                7 => return self.yield_at(8, PUSH_INT, self.item.unwrap()),
                8 => {
                    self.argument = 0;
                    self.phase = 9;
                }
                9 => return self.arguments(),
                10 => {
                    if input.flag == 0 {
                        return action(if filter { THROW_RESULT } else { RETURN }, -1);
                    }
                    return self.yield_at(11, TYPE, -1);
                }
                11 => {
                    self.bool_result = input.kind == BOOL;
                    if self.bool_result {
                        return self.yield_at(12, GET_BOOL, -1);
                    }
                    if !filter && input.kind == INTEGER {
                        return self.yield_at(12, GET_INT, -1);
                    }
                    if filter {
                        return action(THROW_ERROR, 5);
                    }
                    return self.yield_at(
                        19,
                        POP,
                        self.nparam.checked_add(4).expect("defined pop count"),
                    );
                }
                12 => {
                    self.value = if self.bool_result {
                        i64::from(input.flag != 0)
                    } else {
                        input.a
                    };
                    if !filter {
                        if self.token != list.modifications {
                            return self.yield_at(
                                20,
                                POP,
                                self.nparam.checked_add(4).expect("defined pop count"),
                            );
                        }
                        list.set(self.item.unwrap(), self.value);
                    }
                    return self.yield_at(13, POP_TOP, 0);
                }
                13 => {
                    if filter {
                        return if self.value != 0 {
                            self.yield_at(22, INDEX, 0)
                        } else {
                            self.yield_at(6, ITEM, 0)
                        };
                    }
                    return self.yield_at(6, CHARGE, 5);
                }
                21 => {
                    self.item = Some(input.a);
                    if self.nparam < 1 {
                        list.add(input.a, 0);
                        return self.yield_at(6, ITEM, 0);
                    }
                    return self.yield_at(7, ROOT, 0);
                }
                22 => {
                    list.add(input.a, 0);
                    return self.yield_at(6, ITEM, 0);
                }
                18 => return action(RETURN, 0),
                19 => return action(ERROR, 3),
                20 => return action(ERROR, 4),
                _ => std::process::abort(),
            }
        }
    }
    fn save(&mut self, list: &List) -> Action {
        match self.phase {
            0 => self.yield_at(1, TAG, 0),
            1 => self.yield_at(2, ARRAY_NEW, 0),
            2 => self.yield_at(3, PUSH_INT, i64::from(list.sort_by_item)),
            3 => self.yield_at(4, APPEND, -2),
            4 => self.yield_at(5, PUSH_BOOL, i64::from(list.ascending)),
            5 => self.yield_at(6, APPEND, -2),
            6 => self.yield_at(7, TABLE_NEW, 0),
            7 => {
                if let Some((key, value)) = self.entry(list) {
                    self.item = Some(key);
                    self.value = value;
                    self.yield_at(8, PUSH_INT, key)
                } else {
                    self.yield_at(10, APPEND, -2)
                }
            }
            8 => self.yield_at(9, PUSH_INT, self.value),
            9 => self.yield_at(7, RAW_SET, -3),
            10 => action(RETURN, 1),
            _ => std::process::abort(),
        }
    }
    #[allow(clippy::too_many_lines)] // Explicit partial-stack parsing phases without transactional cleanup.
    fn load(&mut self, list: &mut List, input: Input) -> Action {
        match self.phase {
            0 => self.yield_at(1, TYPE, -1),
            1 => {
                if input.kind == ARRAY {
                    self.yield_at(2, PUSH_NULL, 0)
                } else {
                    action(RETURN, 0)
                }
            }
            2 => self.yield_at(3, NEXT, -2),
            3 => {
                if input.flag != 0 {
                    self.yield_at(4, TYPE, -1)
                } else {
                    action(RETURN, 0)
                }
            }
            4 => {
                if input.kind == INTEGER {
                    self.yield_at(5, GET_INT, -1)
                } else {
                    action(RETURN, 0)
                }
            }
            5 => {
                self.sort = Self::sort_kind(input.a);
                self.yield_at(6, POP, 2)
            }
            6 => self.yield_at(7, NEXT, -2),
            7 => {
                if input.flag != 0 {
                    self.yield_at(8, TYPE, -1)
                } else {
                    action(RETURN, 0)
                }
            }
            8 => {
                if input.kind == BOOL {
                    self.yield_at(9, GET_BOOL, -1)
                } else {
                    action(RETURN, 0)
                }
            }
            9 => {
                self.ascending = input.flag == 1;
                self.yield_at(10, POP, 2)
            }
            10 => self.yield_at(11, NEXT, -2),
            11 => {
                if input.flag != 0 {
                    self.yield_at(12, TYPE, -1)
                } else {
                    action(RETURN, 0)
                }
            }
            12 => {
                if input.kind == TABLE {
                    self.yield_at(13, PUSH_NULL, 0)
                } else {
                    action(RETURN, 0)
                }
            }
            13 => self.yield_at(14, NEXT, -2),
            14 => {
                if input.flag != 0 {
                    self.yield_at(15, TYPE, -2)
                } else {
                    self.yield_at(19, POP, 3)
                }
            }
            15 => {
                if input.kind == INTEGER {
                    self.yield_at(18, GET_PAIR, 0)
                } else {
                    self.yield_at(16, TYPE, -1)
                }
            }
            16 => {
                if input.kind == INTEGER {
                    self.yield_at(18, GET_PAIR, 0)
                } else {
                    action(RETURN, 0)
                }
            }
            18 => {
                list.add(input.a, input.b);
                self.yield_at(13, POP, 2)
            }
            19 => self.yield_at(20, NEXT, -2),
            20 => {
                if input.flag != 0 {
                    action(RETURN, 0)
                } else {
                    self.yield_at(21, POP, 1)
                }
            }
            21 => {
                list.sort(self.sort, self.ascending);
                action(RETURN, 1)
            }
            _ => std::process::abort(),
        }
    }
    fn get_set(&mut self, list: &mut List, input: Input, set: bool) -> Action {
        match self.phase {
            0 => self.yield_at(1, TYPE, 2),
            1 => {
                if input.kind == INTEGER {
                    self.yield_at(2, GET_INT, 2)
                } else {
                    action(RETURN, -1)
                }
            }
            2 => {
                self.item = Some(input.a);
                if set {
                    self.yield_at(3, TYPE, 3)
                } else if let Some(&value) = list.items.get(&input.a) {
                    self.yield_at(5, PUSH_INT, value)
                } else {
                    action(RETURN, -1)
                }
            }
            3 => match input.kind {
                NULL => {
                    list.remove(self.item.unwrap());
                    action(RETURN, 0)
                }
                BOOL => {
                    self.bool_result = true;
                    self.yield_at(4, GET_BOOL, 3)
                }
                INTEGER => self.yield_at(4, GET_INT, 3),
                _ => action(ERROR, 6),
            },
            4 => {
                let value = if self.bool_result {
                    i64::from(input.flag != 0)
                } else {
                    input.a
                };
                let key = self.item.unwrap();
                if list.items.contains_key(&key) {
                    list.set(key, value);
                } else {
                    list.add(key, value);
                }
                action(RETURN, 0)
            }
            5 => action(RETURN, 1),
            _ => std::process::abort(),
        }
    }
    fn next(&mut self, list: &mut List, input: Input) -> Action {
        loop {
            match self.phase {
                0 => return self.yield_at(1, TYPE, 2),
                1 => {
                    if input.kind == NULL {
                        return if list.items.is_empty() {
                            self.yield_at(5, PUSH_NULL, 0)
                        } else {
                            let key = list.begin();
                            self.yield_at(5, PUSH_INT, key)
                        };
                    }
                    return self.yield_at(2, GET_INT, 2);
                }
                2 => {
                    self.value = list.next();
                    self.phase = 3;
                    if !list.initialized {
                        return action(LOG_NEXT, 0);
                    }
                }
                3 => {
                    if !list.initialized {
                        return self.yield_at(4, LOG_END, 0);
                    }
                    return if list.items.is_empty() || list.ended {
                        self.yield_at(5, PUSH_NULL, 0)
                    } else {
                        self.yield_at(5, PUSH_INT, self.value)
                    };
                }
                4 => return self.yield_at(5, PUSH_NULL, 0),
                5 => return action(RETURN, 1),
                _ => std::process::abort(),
            }
        }
    }
    fn step(&mut self, list: &mut List, input: Input) -> Action {
        match self.operation {
            0 => self.callback(list, input, false),
            1 => self.callback(list, input, true),
            2 => self.save(list),
            3 => self.load(list, input),
            4 => self.get_set(list, input, false),
            5 => self.get_set(list, input, true),
            6 => self.next(list, input),
            _ => std::process::abort(),
        }
    }
}

// Controller/list/output pointers are live, aligned and disjoint. Each scalar
// input is copied before creating exclusive borrows, which end before return.
// Neither destruction nor step invokes a host callback, VM function or cleanup.
#[allow(unsafe_code)]
mod ffi {
    use super::{Action, Control, Input, List};
    #[unsafe(no_mangle)]
    pub(crate) extern "C" fn openttd_rust_list_control_new(operation: u8) -> *mut Control {
        Box::into_raw(Box::new(Control::new(operation)))
    }
    #[unsafe(no_mangle)]
    pub(crate) unsafe extern "C" fn openttd_rust_list_control_destroy(control: *mut Control) {
        unsafe {
            drop(Box::from_raw(control));
        }
    }
    #[unsafe(no_mangle)]
    pub(crate) unsafe extern "C" fn openttd_rust_list_control_step(
        control: *mut Control,
        list: *mut List,
        input: *const Input,
        output: *mut Action,
    ) {
        unsafe {
            let input = input.read();
            output.write((&mut *control).step(&mut *list, input));
        }
    }
}
