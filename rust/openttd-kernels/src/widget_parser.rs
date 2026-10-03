/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Descriptor traversal only; all polymorphic widget operations run after return.

pub fn attribute(kind: u8) -> bool {
    kind > 36 && kind < 50
}

pub fn container(kind: u8) -> bool {
    matches!(kind, 24 | 25 | 26 | 27 | 1 | 13 | 2 | 29 | 30)
}

/// Widget-only instruction pinned by `widget_parser_ffi.h`.
#[repr(C)]
#[derive(Default)]
pub struct Action {
    /// Borrowed descriptor offset, never a pointer into a C++ union.
    pub part: u64,
    /// Stable C++ owner slot.
    pub slot: u64,
    /// Attachment source slot.
    pub other: u64,
    /// Operation selector.
    pub kind: u8,
}

const DONE: u8 = 0;
const CHECK_NONNULL: u8 = 1;
const CREATE_VERTICAL: u8 = 2;
const OBSERVE_PARENT: u8 = 3;
const READ_PART: u8 = 4;
const CREATE_NODE: u8 = 5;
const APPLY_ATTRIBUTE: u8 = 6;
const OBSERVE_TYPE: u8 = 7;
const ADD_CONTAINER: u8 = 8;
const ADD_BACKGROUND: u8 = 9;
const ASSIGN_PARENT: u8 = 10;
const RELEASE: u8 = 11;
const ASSERT_END: u8 = 12;
const ERROR_ATTRIBUTE: u8 = 13;
const ERROR_TRAILING: u8 = 14;
const ASSERT_FIRST: u8 = 15;
const OBSERVE_HORIZONTAL: u8 = 16;
const QUERY_CAPTION: u8 = 17;
const QUERY_SHADE: u8 = 18;
const CREATE_SHADE: u8 = 19;
const WRITE_SHADE: u8 = 20;

#[derive(Clone, Copy)]
enum Stage {
    Enter,
    Parent,
    Loop,
    Node,
    Created,
    NullChild,
    Attributes,
    AttributeType,
    AttributeApplied,
    Descend,
    ObjectType,
    ChildFinished,
    Attach,
    ContainerAdded,
    BackgroundAdded,
    Assigned,
    Return,
    EndChecked,
}

struct Frame {
    slot: u64,
    child: u64,
    capabilities: u8,
    fill: bool,
    stage: Stage,
}

#[derive(Clone, Copy)]
enum Role {
    Main,
    First,
    Body,
    ShadeBody,
}

#[derive(Clone, Copy)]
enum Control {
    Init,
    InitialChecked,
    DefaultMade,
    Walk,
    Parsed,
    FirstAsserted,
    HorizontalObserved,
    RootMade,
    FirstAdded,
    FirstReleased,
    CaptionChecked,
    ShadeChecked,
    ShadeMade,
    ShadeWritten,
    BodyMade,
    BodyAdded,
    BodyReleased,
    ShadeAdded,
    ShadeReleased,
    Done,
}

pub struct Engine {
    length: u64,
    position: u64,
    next_slot: u64,
    root: u64,
    shade: u64,
    body: u64,
    horizontal: bool,
    trailing_check: bool,
    role: Role,
    control: Control,
    frames: Vec<Frame>,
}

impl Engine {
    pub fn new(length: u64, window: bool, trailing_check: bool) -> Self {
        Self {
            length,
            position: 0,
            next_slot: 1,
            root: 0,
            shade: 0,
            body: 0,
            horizontal: false,
            trailing_check,
            role: if window { Role::First } else { Role::Main },
            control: Control::Init,
            frames: Vec::new(),
        }
    }

    fn action(&self, kind: u8, slot: u64, other: u64) -> Action {
        Action {
            part: self.position,
            slot,
            other,
            kind,
        }
    }

    fn allocate_slot(&mut self) -> u64 {
        let slot = self.next_slot;
        self.next_slot = slot
            .checked_add(1)
            .expect("representable widget owner slot");
        slot
    }

    fn start_tree(&mut self, slot: u64) {
        self.control = Control::Walk;
        self.frames.push(Frame {
            slot,
            child: 0,
            capabilities: 0,
            fill: false,
            stage: Stage::Enter,
        });
    }

    fn finish_tree(&mut self) {
        self.frames.pop().expect("live parser frame");
        if self.frames.is_empty() {
            self.control = Control::Parsed;
        }
    }

    fn finish_body(&mut self) -> Option<Action> {
        if self.trailing_check && self.position != self.length {
            return Some(self.action(ERROR_TRAILING, 0, 0));
        }
        if matches!(self.role, Role::ShadeBody) {
            self.control = Control::BodyAdded;
            Some(self.action(ADD_CONTAINER, self.shade, self.body))
        } else {
            self.control = Control::Done;
            None
        }
    }

    fn start_body(&mut self) {
        self.role = Role::Body;
        self.start_tree(self.root);
    }

    fn compose(&mut self, response: u8) -> Option<Action> {
        match self.control {
            Control::Init if matches!(self.role, Role::Main) => {
                self.control = Control::InitialChecked;
                return Some(self.action(CHECK_NONNULL, 0, 0));
            }
            Control::InitialChecked if response == 0 => {
                self.control = Control::DefaultMade;
                return Some(self.action(CREATE_VERTICAL, 0, 0));
            }
            Control::Init | Control::InitialChecked | Control::DefaultMade => self.start_tree(0),
            Control::Parsed if !matches!(self.role, Role::First) => return self.finish_body(),
            Control::Parsed => {
                self.control = Control::FirstAsserted;
                return Some(self.action(ASSERT_FIRST, 0, 0));
            }
            Control::FirstAsserted => {
                self.control = Control::HorizontalObserved;
                return Some(self.action(OBSERVE_HORIZONTAL, 0, 0));
            }
            Control::HorizontalObserved => {
                self.horizontal = response != 0;
                self.root = self.allocate_slot();
                self.control = Control::RootMade;
                return Some(self.action(CREATE_VERTICAL, self.root, 0));
            }
            Control::RootMade => {
                self.control = Control::FirstAdded;
                return Some(self.action(ADD_CONTAINER, self.root, 0));
            }
            Control::FirstAdded => {
                self.control = Control::FirstReleased;
                return Some(self.action(RELEASE, 0, 0));
            }
            Control::FirstReleased if self.position == self.length => self.control = Control::Done,
            Control::FirstReleased if self.horizontal => {
                self.control = Control::CaptionChecked;
                return Some(self.action(QUERY_CAPTION, 0, 0));
            }
            Control::CaptionChecked if response != 0 => {
                self.control = Control::ShadeChecked;
                return Some(self.action(QUERY_SHADE, 0, 0));
            }
            Control::ShadeChecked if response != 0 => {
                self.shade = self.allocate_slot();
                self.control = Control::ShadeMade;
                return Some(self.action(CREATE_SHADE, self.shade, 0));
            }
            Control::FirstReleased | Control::CaptionChecked | Control::ShadeChecked => {
                self.start_body();
            }
            Control::ShadeMade => {
                self.control = Control::ShadeWritten;
                return Some(self.action(WRITE_SHADE, self.shade, 0));
            }
            Control::ShadeWritten => {
                self.body = self.allocate_slot();
                self.control = Control::BodyMade;
                return Some(self.action(CREATE_VERTICAL, self.body, 0));
            }
            Control::BodyMade => {
                self.role = Role::ShadeBody;
                self.start_tree(self.body);
            }
            Control::BodyAdded => {
                self.control = Control::BodyReleased;
                return Some(self.action(RELEASE, self.body, 0));
            }
            Control::BodyReleased => {
                self.control = Control::ShadeAdded;
                return Some(self.action(ADD_CONTAINER, self.root, self.shade));
            }
            Control::ShadeAdded => {
                self.control = Control::ShadeReleased;
                return Some(self.action(RELEASE, self.shade, 0));
            }
            Control::ShadeReleased => self.control = Control::Done,
            Control::Done => return Some(self.action(DONE, self.root, 0)),
            Control::Walk => {}
        }
        None
    }

    fn begin_node(&mut self, top: usize, response: u8) -> Option<Action> {
        let slot = self.frames[top].slot;
        match self.frames[top].stage {
            Stage::Enter => {
                self.frames[top].stage = Stage::Parent;
                return Some(self.action(OBSERVE_PARENT, slot, 0));
            }
            Stage::Parent => {
                self.frames[top].capabilities = response;
                self.frames[top].stage = Stage::Loop;
            }
            Stage::Loop if self.position == self.length => self.finish_tree(),
            Stage::Loop => {
                self.frames[top].stage = Stage::Node;
                return Some(self.action(READ_PART, slot, 0));
            }
            Stage::Node if attribute(response) => {
                return Some(self.action(ERROR_ATTRIBUTE, slot, 0));
            }
            Stage::Node if response == 52 => {
                self.frames[top].stage = Stage::EndChecked;
                return Some(self.action(ASSERT_END, slot, 0));
            }
            Stage::Node => {
                self.frames[top].fill = container(response);
                let child = self.allocate_slot();
                self.frames[top].child = child;
                self.frames[top].stage = Stage::Created;
                return Some(self.action(CREATE_NODE, child, 0));
            }
            Stage::Created if response == 0 => {
                self.frames[top].stage = Stage::NullChild;
                return Some(self.action(RELEASE, self.frames[top].child, 0));
            }
            Stage::Created | Stage::AttributeApplied => {
                self.position += 1;
                self.frames[top].stage = Stage::Attributes;
            }
            Stage::NullChild => {
                self.frames[top].stage = Stage::EndChecked;
                return Some(self.action(ASSERT_END, slot, 0));
            }
            Stage::Attributes if self.position == self.length => {
                self.frames[top].stage = Stage::Descend;
            }
            Stage::Attributes => {
                self.frames[top].stage = Stage::AttributeType;
                return Some(self.action(READ_PART, slot, 0));
            }
            Stage::AttributeType if attribute(response) => {
                self.frames[top].stage = Stage::AttributeApplied;
                return Some(self.action(APPLY_ATTRIBUTE, self.frames[top].child, 0));
            }
            Stage::AttributeType => self.frames[top].stage = Stage::Descend,
            _ => unreachable!("node entry stages dispatched separately"),
        }
        None
    }

    fn finish_node(&mut self, top: usize, response: u8) -> Option<Action> {
        let slot = self.frames[top].slot;
        let child = self.frames[top].child;
        let capabilities = self.frames[top].capabilities;
        match self.frames[top].stage {
            Stage::Descend if self.frames[top].fill => {
                self.frames[top].stage = Stage::ObjectType;
                return Some(self.action(OBSERVE_TYPE, child, 0));
            }
            Stage::ObjectType if container(response) => {
                self.frames[top].stage = Stage::ChildFinished;
                self.start_tree(child);
            }
            Stage::Descend | Stage::ObjectType | Stage::ChildFinished => {
                self.frames[top].stage = Stage::Attach;
            }
            Stage::Attach => {
                self.frames[top].stage = Stage::ContainerAdded;
                if capabilities & 2 != 0 {
                    return Some(self.action(ADD_CONTAINER, slot, child));
                }
            }
            Stage::ContainerAdded => {
                self.frames[top].stage = Stage::BackgroundAdded;
                if capabilities & 4 != 0 {
                    return Some(self.action(ADD_BACKGROUND, slot, child));
                }
            }
            Stage::BackgroundAdded if capabilities & 6 == 0 => {
                self.frames[top].stage = Stage::Assigned;
                return Some(self.action(ASSIGN_PARENT, slot, child));
            }
            Stage::BackgroundAdded => {
                self.frames[top].stage = Stage::Loop;
                return Some(self.action(RELEASE, child, 0));
            }
            Stage::Assigned => {
                self.frames[top].stage = Stage::Return;
                return Some(self.action(RELEASE, child, 0));
            }
            Stage::Return => self.finish_tree(),
            Stage::EndChecked => {
                self.position += 1;
                self.finish_tree();
            }
            _ => unreachable!("node completion stages dispatched separately"),
        }
        None
    }

    pub fn advance(&mut self, response: u8) -> Action {
        loop {
            if let Some(action) = self.compose(response) {
                return action;
            }
            if !matches!(self.control, Control::Walk) {
                continue;
            }
            let top = self.frames.len() - 1;
            let action = match self.frames[top].stage {
                Stage::Enter
                | Stage::Parent
                | Stage::Loop
                | Stage::Node
                | Stage::Created
                | Stage::NullChild
                | Stage::Attributes
                | Stage::AttributeType
                | Stage::AttributeApplied => self.begin_node(top, response),
                _ => self.finish_node(top, response),
            };
            if let Some(action) = action {
                return action;
            }
        }
    }
}
