/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Admin conversion traversal; typed VM/JSON effects execute between Rust calls.

/// Scalar instruction whose C++ execution may invoke user code or throw.
#[repr(C)]
#[derive(Default)]
pub struct Action {
    /// Stable LIFO typed-frame slot, not a pointer.
    pub frame: u64,
    /// Signed VM index, or original saved stack top.
    pub index: i64,
    /// Operation pinned by `admin_conversion_ffi.h`.
    pub kind: u8,
    /// Scalar/container/cleanup selector.
    pub argument: u8,
}

const DONE: u8 = 0;
const INSPECT_OUT: u8 = 1;
const INSPECT_IN: u8 = 2;
const COPY_OUT_SCALAR: u8 = 3;
const PUSH_IN_SCALAR: u8 = 4;
const BEGIN_OUT: u8 = 5;
const BEGIN_IN: u8 = 6;
const NEXT_OUT: u8 = 7;
const CHECK_IN_ITERATOR: u8 = 8;
const COPY_OUT_KEY: u8 = 9;
const PUSH_IN_KEY: u8 = 10;
const CHILD_OUT: u8 = 11;
const CHILD_IN: u8 = 12;
const POP_PAIR: u8 = 13;
const POP_ITERATOR: u8 = 14;
const COMMIT_OUT_ARRAY: u8 = 15;
const COMMIT_OUT_TABLE: u8 = 16;
const COMMIT_IN_ARRAY: u8 = 17;
const COMMIT_IN_TABLE: u8 = 18;
const ADVANCE_IN_ITERATOR: u8 = 19;
const RELEASE_FRAME: u8 = 20;
const ERROR_DEPTH: u8 = 21;
const ERROR_OUT_TYPE: u8 = 22;
const ERROR_ROOT: u8 = 23;
const ERROR_IN_TYPE: u8 = 24;
const SAVE_TOP: u8 = 25;
const RESTORE_TOP: u8 = 26;
const PUSH_NULL: u8 = 27;

#[derive(Clone, Copy)]
enum Stage {
    Enter,
    Inspected,
    Scalar,
    Begun,
    Iterated,
    Key,
    Child,
    ChildFinished(bool),
    PairPopped(bool),
    Committed,
    Released,
    IteratorPopped(bool),
    Failed,
    InCommitted,
    InAdvanced,
}

struct Frame {
    index: i64,
    depth: i32,
    table: bool,
    stage: Stage,
}

#[derive(Clone, Copy)]
enum Control {
    Root,
    RootInspected,
    TopSaved,
    Walk,
    RootError,
    RootNull,
    Finished(bool),
    Restore,
    Invalid,
    InvalidNull,
    Done(bool),
}

pub struct Engine {
    incoming: bool,
    frames: Vec<Frame>,
    control: Control,
    top: i64,
}

impl Engine {
    pub fn new(direction: u8, index: i64, depth: i32) -> Self {
        assert!(direction <= 1);
        let incoming = direction != 0;
        Self {
            incoming,
            frames: vec![Frame {
                index,
                depth,
                table: false,
                stage: Stage::Enter,
            }],
            control: if incoming {
                Control::Root
            } else {
                Control::Walk
            },
            top: 0,
        }
    }

    fn action(&self, kind: u8, argument: u8) -> Action {
        let slot = self.frames.len().saturating_sub(1);
        Action {
            frame: u64::try_from(slot).expect("native frame slot"),
            index: self.frames.last().map_or(0, |frame| frame.index),
            kind,
            argument,
        }
    }

    fn finish(&mut self, result: bool) -> Option<Action> {
        let slot = self.frames.len() - 1;
        self.frames.pop();
        if let Some(parent) = self.frames.last_mut() {
            parent.stage = Stage::ChildFinished(result);
            if !self.incoming {
                return None;
            }
        } else {
            self.control = Control::Finished(result);
        }
        Some(Action {
            frame: u64::try_from(slot).expect("native frame slot"),
            kind: RELEASE_FRAME,
            ..Action::default()
        })
    }

    fn advance_control(&mut self, response: u64) -> Option<Action> {
        match self.control {
            Control::Root => {
                self.control = Control::RootInspected;
                return Some(self.action(INSPECT_IN, 0));
            }
            Control::RootInspected => {
                if response == 1 {
                    // nlohmann object, statically pinned in C++.
                    self.control = Control::TopSaved;
                    return Some(self.action(SAVE_TOP, 0));
                }
                self.control = Control::RootError;
                return Some(self.action(ERROR_ROOT, 0));
            }
            Control::TopSaved => {
                self.top = i64::from_ne_bytes(response.to_ne_bytes());
                self.control = Control::Walk;
            }
            Control::RootError => {
                self.control = Control::RootNull;
                return Some(self.action(PUSH_NULL, 0));
            }
            Control::RootNull => {
                self.frames.clear();
                self.control = Control::Done(false);
                return Some(Action {
                    kind: RELEASE_FRAME,
                    ..Action::default()
                });
            }
            Control::Finished(result) => {
                if self.incoming && !result {
                    self.control = Control::Restore;
                    return Some(Action {
                        kind: RESTORE_TOP,
                        index: self.top,
                        ..Action::default()
                    });
                }
                self.control = Control::Done(result);
            }
            Control::Restore => {
                self.control = Control::Invalid;
                return Some(Action {
                    kind: ERROR_IN_TYPE,
                    ..Action::default()
                });
            }
            Control::Invalid => {
                self.control = Control::InvalidNull;
                return Some(Action {
                    kind: PUSH_NULL,
                    ..Action::default()
                });
            }
            Control::InvalidNull => self.control = Control::Done(false),
            Control::Done(result) => {
                return Some(Action {
                    kind: DONE,
                    argument: u8::from(result),
                    ..Action::default()
                });
            }
            Control::Walk => {}
        }
        None
    }

    fn inspect(&mut self, slot: usize, response: u64) -> Option<Action> {
        // Exact raw type values, not a C++ acceptance predicate.
        let scalar = if self.incoming {
            match response {
                0 => Some(0),
                4 => Some(1),
                3 => Some(2),
                5 | 6 => Some(3),
                _ => None,
            }
        } else {
            match response {
                0x0100_0001 => Some(0),
                0x0100_0008 => Some(1),
                0x0800_0010 => Some(2),
                0x0500_0002 => Some(3),
                _ => None,
            }
        };
        if let Some(kind) = scalar {
            self.frames[slot].stage = Stage::Scalar;
            return Some(self.action(
                if self.incoming {
                    PUSH_IN_SCALAR
                } else {
                    COPY_OUT_SCALAR
                },
                kind,
            ));
        }
        let container = if self.incoming {
            match response {
                1 => Some(true),
                2 => Some(false),
                _ => None,
            }
        } else {
            match response {
                0x0a00_0020 => Some(true),
                0x0800_0040 => Some(false),
                _ => None,
            }
        };
        if let Some(table) = container {
            self.frames[slot].table = table;
            self.frames[slot].stage = Stage::Begun;
            return Some(self.action(
                if self.incoming { BEGIN_IN } else { BEGIN_OUT },
                u8::from(table),
            ));
        }
        self.frames[slot].stage = Stage::Failed;
        if !self.incoming {
            return Some(self.action(ERROR_OUT_TYPE, 0));
        }
        None
    }

    fn traverse(&mut self, slot: usize, response: u64) -> Option<Action> {
        match self.frames[slot].stage {
            Stage::Begun | Stage::Released => {
                self.frames[slot].stage = Stage::Iterated;
                if self.incoming {
                    return Some(self.action(CHECK_IN_ITERATOR, 0));
                }
                let mut action = self.action(NEXT_OUT, 0);
                action.index = action
                    .index
                    .checked_sub(1)
                    .expect("original valid VM index");
                return Some(action);
            }
            Stage::Iterated => {
                if response == 0 {
                    if self.incoming {
                        if let Some(action) = self.finish(true) {
                            return Some(action);
                        }
                    } else {
                        self.frames[slot].stage = Stage::IteratorPopped(true);
                        return Some(self.action(POP_ITERATOR, 0));
                    }
                } else if self.frames[slot].table {
                    self.frames[slot].stage = Stage::Key;
                    return Some(self.action(
                        if self.incoming {
                            PUSH_IN_KEY
                        } else {
                            COPY_OUT_KEY
                        },
                        0,
                    ));
                } else {
                    self.frames[slot].stage = Stage::Key;
                }
            }
            Stage::Key => {
                self.frames[slot].stage = Stage::Child;
                let depth = if self.incoming {
                    0
                } else {
                    self.frames[slot]
                        .depth
                        .checked_add(1)
                        .expect("original defined depth increment")
                };
                self.frames.push(Frame {
                    index: -1,
                    depth,
                    table: false,
                    stage: Stage::Enter,
                });
                return Some(self.action(if self.incoming { CHILD_IN } else { CHILD_OUT }, 0));
            }
            Stage::Child => unreachable!("child must finish before its parent"),
            _ => unreachable!("entry and completion stages dispatched separately"),
        }
        None
    }

    fn complete(&mut self, slot: usize) -> Option<Action> {
        match self.frames[slot].stage {
            Stage::ChildFinished(result) => {
                if self.incoming {
                    if result {
                        self.frames[slot].stage = Stage::InCommitted;
                        return Some(self.action(
                            if self.frames[slot].table {
                                COMMIT_IN_TABLE
                            } else {
                                COMMIT_IN_ARRAY
                            },
                            0,
                        ));
                    }
                    if let Some(action) = self.finish(false) {
                        return Some(action);
                    }
                } else {
                    self.frames[slot].stage = Stage::PairPopped(result);
                    return Some(self.action(POP_PAIR, 0));
                }
            }
            Stage::PairPopped(result) => {
                if result {
                    self.frames[slot].stage = Stage::Committed;
                    return Some(self.action(
                        if self.frames[slot].table {
                            COMMIT_OUT_TABLE
                        } else {
                            COMMIT_OUT_ARRAY
                        },
                        0,
                    ));
                }
                self.frames[slot].stage = Stage::IteratorPopped(false);
                return Some(self.action(POP_ITERATOR, 0));
            }
            Stage::Committed => {
                self.frames[slot].stage = Stage::Released;
                return Some(Action {
                    frame: u64::try_from(slot + 1).expect("native frame slot"),
                    kind: RELEASE_FRAME,
                    argument: 1,
                    ..Action::default()
                });
            }
            Stage::IteratorPopped(result) => {
                if !result {
                    self.frames[slot].stage = Stage::Failed;
                    return Some(Action {
                        frame: u64::try_from(slot + 1).expect("native frame slot"),
                        kind: RELEASE_FRAME,
                        argument: 1,
                        ..Action::default()
                    });
                }
                if let Some(action) = self.finish(true) {
                    return Some(action);
                }
            }
            Stage::InCommitted => {
                self.frames[slot].stage = Stage::InAdvanced;
                return Some(self.action(ADVANCE_IN_ITERATOR, 0));
            }
            Stage::InAdvanced => {
                self.frames[slot].stage = Stage::Begun;
            }
            _ => unreachable!("entry and completion stages dispatched separately"),
        }
        None
    }

    pub fn advance(&mut self, response: u64) -> Action {
        loop {
            if let Some(action) = self.advance_control(response) {
                return action;
            }
            if !matches!(self.control, Control::Walk) {
                continue;
            }
            let slot = self.frames.len() - 1;
            let action = match self.frames[slot].stage {
                Stage::Enter => {
                    if !self.incoming && self.frames[slot].depth == 25 {
                        self.frames[slot].stage = Stage::Failed;
                        Some(self.action(ERROR_DEPTH, 0))
                    } else {
                        self.frames[slot].stage = Stage::Inspected;
                        Some(self.action(
                            if self.incoming {
                                INSPECT_IN
                            } else {
                                INSPECT_OUT
                            },
                            0,
                        ))
                    }
                }
                Stage::Inspected => self.inspect(slot, response),
                Stage::Scalar => self.finish(true),
                Stage::Failed => self.finish(false),
                Stage::Begun | Stage::Released | Stage::Iterated | Stage::Key | Stage::Child => {
                    self.traverse(slot, response)
                }
                _ => self.complete(slot),
            };
            if let Some(action) = action {
                return action;
            }
        }
    }
}
