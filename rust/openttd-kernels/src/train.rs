/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete train control. Canonical private fields are accessed only for one scalar
//! operation; no Rust reference into an owner or world object crosses a callback.
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
    clippy::collapsible_if,
    clippy::verbose_bit_mask
)]
use crate::services::Services;
use crate::train_state::State;
const INVALID: u32 = u32::MAX;
const DEPOT: u8 = 0x80;
const WORMHOLE: u8 = 0x40;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct View {
    pub id: u32,
    pub first: u32,
    pub next: u32,
    pub previous: u32,
    pub next_unit: u32,
    pub last: u32,
    pub tile: u32,
    pub dest: u32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub order_time: i32,
    pub power: u32,
    pub weight: u32,
    pub length: u16,
    pub total_length: u16,
    pub max_speed: u16,
    pub max_track_speed: u16,
    pub speed: u16,
    pub gv_flags: u16,
    pub cargo_cap: u16,
    pub refit_cap: u16,
    pub engine: u16,
    pub first_engine: u16,
    pub order_destination: u16,
    pub last_station: u16,
    pub direction: u8,
    pub status: u8,
    pub tick: u8,
    pub running: u8,
    pub day: u8,
    pub progress: u8,
    pub subspeed: u8,
    pub acceleration: u8,
    pub order: u8,
    pub nonstop: u8,
    pub breakdown: u8,
    pub front: u8,
    pub free_wagon: u8,
    pub articulated: u8,
    pub engine_part: u8,
    pub multiheaded: u8,
    pub owner: u8,
    pub vis_effect: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub observe: extern "C" fn(u32, *mut View),
    pub write: extern "C" fn(u32, u32, u64),
    pub leaf: extern "C" fn(u32, u32, u64, u64, u64) -> u64,
    pub owner: extern "C" fn(u32) -> *mut State,
    pub nearby: extern "C" fn(u32, u32, i32, i32, *mut u32, usize) -> usize,
}
#[derive(Clone)]
struct Game {
    leaves: Leaves,
    services: Services,
    mailbox: std::rc::Rc<Mailbox>,
}
impl Game {
    fn read(&self, id: u32) -> View {
        let mut v = View::default();
        (self.leaves.observe)(id, &raw mut v);
        v
    }
    fn write(&self, id: u32, field: u32, value: u64) {
        (self.leaves.write)(id, field, value);
    }
    fn leaf(&self, op: u32, id: u32, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.leaf)(op, id, a, b, c)
    }
    fn op(&self, op: u32, id: u32) -> u64 {
        self.leaf(op, id, 0, 0, 0)
    }
    fn val(&self, op: u32, id: u32, a: u64) -> u64 {
        self.leaf(op, id, a, 0, 0)
    }
    fn get(&self, id: u32, field: u8) -> u64 {
        // SAFETY: The game thread supplies the live shell; borrow ends in this accessor.
        unsafe { crate::train_state::get((self.leaves.owner)(id), field) }
    }
    fn set(&self, id: u32, field: u8, value: u64) {
        // SAFETY: Serialized live owner; no owner reference survives this operation.
        unsafe {
            crate::train_state::set((self.leaves.owner)(id), field, value);
        }
    }
    fn flag(&self, id: u32, bit: u8) -> bool {
        self.get(id, 0) & (1 << bit) != 0
    }
    fn set_flag(&self, id: u32, bit: u8, value: bool) {
        let mask = 1 << bit;
        let old = self.get(id, 0);
        self.set(id, 0, if value { old | mask } else { old & !mask });
    }
}
const W_TILE: u32 = 0;
const W_DEST: u32 = 1;
const W_X: u32 = 2;
const W_Y: u32 = 3;
const W_Z: u32 = 4;
const W_DIRECTION: u32 = 5;
const W_SPEED: u32 = 6;
const W_TICK: u32 = 7;
const W_RUNNING: u32 = 8;
const W_DAY: u32 = 9;
const W_ORDER_TIME: u32 = 10;
const W_PROGRESS: u32 = 11;
const W_SUBSPEED: u32 = 12;
const W_GV_FLAGS: u32 = 13;
const W_ACCELERATION: u32 = 14;
const W_LENGTH: u32 = 15;
const W_TOTAL_LENGTH: u32 = 16;
const W_FIRST_ENGINE: u32 = 17;
const W_MAX_SPEED: u32 = 18;
const W_CARGO_CAP: u32 = 19;
const W_REFIT_CAP: u32 = 20;
const W_CARGO_AGE: u32 = 21;
const W_LAST_STATION: u32 = 22;
const W_COLOURMAP: u32 = 23;
const W_STATUS: u32 = 24;
const ACC_MODEL: u32 = 0;
const RAIL_TILT: u32 = 1;
const CURVE_MOD: u32 = 2;
const RAIL_TYPES: u32 = 3;
const USER_DEFAULT: u32 = 4;
const POW_WAG_POWER: u32 = 5;
const RAILVEH_WAGON: u32 = 6;
const ENGINE_POWER: u32 = 7;
const WAGON_OVERRIDE: u32 = 8;
const WAGON_SPEED_LIMITS: u32 = 9;
const SPEED_DEFAULT: u32 = 10;
const ALL_POWERED: u32 = 11;
const CARGO_AGE_DEFAULT: u32 = 13;
const GRF_VERSION: u32 = 14;
const LENGTH_CALLBACK: u32 = 15;
const LENGTH_DEFAULT: u32 = 16;
const INVALIDATE_GRF: u32 = 17;
const CACHE_OVERRIDE: u32 = 18;
const VIS_EFFECT: u32 = 19;
const PROPERTY: u32 = 20;
const CAPACITY: u32 = 21;
const TRUNCATE_CARGO: u32 = 22;
const CAPACITY_ERROR: u32 = 23;
const LENGTH_ERROR: u32 = 24;
const CALLBACK_LENGTH: u32 = 25;
const LENGTH_CHANGED: u32 = 26;
const CARGO_CHANGED: u32 = 27;
const CONSIST_WINDOWS: u32 = 28;
const CURVE_ADVANTAGE: u32 = 29;
const IS_STATION: u32 = 30;
const STATION: u32 = 31;
const ORDER_STOP: u32 = 32;
const PLATFORM_AHEAD: u32 = 33;
const PLATFORM_LENGTH: u32 = 34;
const STOP_LOCATION: u32 = 35;
const BRIDGE_SPEED: u32 = 36;
const ORDER_MAX_SPEED: u32 = 37;
const ACCELERATION: u32 = 38;
const UPDATE_SPEED: u32 = 39;
const VIEWPORT: u32 = 40;
const POSITION: u32 = 41;
const INCLINATION: u32 = 42;
const AGE: u32 = 43;
const ECONOMY_AGE: u32 = 44;
const DECREASE_VALUE: u32 = 45;
const CHECK_BREAKDOWN: u32 = 46;
const CHECK_ORDERS: u32 = 47;
const SERVINT: u32 = 48;
const NEEDS_SERVICE: u32 = 49;
const CHAIN_DEPOT: u32 = 50;
const SERVICE: u32 = 51;
const MAX_DEPOT_PENALTY: u32 = 52;
const DEPOT_INDEX: u32 = 53;
const ORDER_DUMMY: u32 = 54;
const ORDER_DEPOT_SERVICE: u32 = 55;
const SUPPRESS_IMPLICIT: u32 = 56;
const START_STOP_DIRTY: u32 = 57;
const STATION_DEST: u32 = 58;
const COST_CLASS: u32 = 59;
const COST_DEFAULT: u32 = 60;
const PRICE: u32 = 61;
const PAY_RUNNING: u32 = 62;
const RUNNING_WINDOWS: u32 = 63;
const COST_DIVISOR: u32 = 64;
const INVALID_PRICE: u32 = 65;
const IS_DEPOT: u32 = 66;
const DEPOT_DIR: u32 = 67;
const TUNNEL_DIR: u32 = 68;
const TRACK_DIRECTION: u32 = 69;
const DIAG_TRACKDIR: u32 = 70;
const DIR_DIAG: u32 = 71;
const FIRST_TRACK: u32 = 72;
const PROP_TRAIN_USER_DATA: u32 = 73;
const PROP_TRAIN_SPEED: u32 = 74;
const PROP_TRAIN_CARGO_AGE_PERIOD: u32 = 75;
const PROP_TRAIN_SHORTEN_FACTOR: u32 = 76;
const PROP_TRAIN_RUNNING_COST_FACTOR: u32 = 77;
const IS_TUNNELBRIDGE: u32 = 78;
const IS_BRIDGE: u32 = 79;
const IS_RAILWAY: u32 = 80;
const IS_PLAIN_RAIL: u32 = 81;
const IS_CROSSING: u32 = 82;
const MAP_SIZE: u32 = 84;
const VEH_EXIT_DIR: u32 = 85;
const TILE_ADD_DIAG: u32 = 86;
const TILE_OFFSET_DIAG: u32 = 87;
const TILE_VIRT: u32 = 88;
const TRACKDIR_EXIT: u32 = 89;
const DIAG_AXIS: u32 = 90;
const AXIS_DIAG: u32 = 91;
const CROSSING_ROAD_AXIS: u32 = 92;
const CROSSING_RAIL_AXIS: u32 = 93;
const CROSSING_RESERVED: u32 = 94;
const CROSSING_BARRED: u32 = 95;
const WRITE_CROSSING_RES: u32 = 96;
const WRITE_CROSSING_BAR: u32 = 97;
const DIRTY_TILE: u32 = 98;
const CROSSING_SOUND: u32 = 99;
const AMBIENT_SOUND: u32 = 100;
const COMPATIBLE_RAIL_OWNER: u32 = 101;
const RAIL_TYPE: u32 = 102;
const TILE_RAIL_TYPE: u32 = 103;
const SIGNALS_UPDATE: u32 = 104;
const SIGNALS_UPDATE_OWNER: u32 = 105;
const RESERVE_PATHS: u32 = 106;
const NO_90: u32 = 107;
const TRACK_CROSSES: u32 = 108;
const TRACK_BITS: u32 = 109;
const TRACKDIR_REACHES: u32 = 110;
const DIAG_REACHES_TRACKS: u32 = 111;
const TRACK_STATUS: u32 = 112;
const DIAG_BETWEEN: u32 = 113;
const HAS_SIGNAL_TD: u32 = 114;
const HAS_SIGNAL: u32 = 115;
const SIGNAL_TYPE: u32 = 116;
const SIGNAL_PBS: u32 = 117;
const SIGNAL_HAS_PBS: u32 = 118;
const ONEWAY_BLOCKING: u32 = 119;
const HAS_SIGNALS: u32 = 120;
const SET_SIGNAL_STATE: u32 = 121;
const SHOW_RESERVATION: u32 = 122;
const HAS_DEPOT_RES: u32 = 123;
const SET_DEPOT_RES: u32 = 124;
const TRY_RESERVE: u32 = 125;
const HAS_RESERVED: u32 = 126;
const UNRESERVE: u32 = 127;
const OTHER_END: u32 = 128;
const SET_TUNNEL_RES: u32 = 129;
const SET_PLATFORM_RES: u32 = 130;
const STATION_AXIS: u32 = 131;
const STATION_COMPATIBLE: u32 = 132;
const SIGNALS_BOTH: u32 = 133;
const BACKOFF: u32 = 134;
const REVERSE_AT_SIGNALS: u32 = 135;
const WAIT_ONEWAY: u32 = 136;
const WAIT_TWOWAY: u32 = 137;
const WAIT_PBS: u32 = 138;
const DAY_TICKS: u32 = 139;
const SIGSEG_PBS: u32 = 140;
const SIGSEG_FULL: u32 = 141;
const ACC_TYPE: u32 = 142;
const WAIT_UNBUNCH: u32 = 143;
const LEAVE_UNBUNCH: u32 = 144;
const RESET_UNBUNCH: u32 = 145;
const LAST_SPEED: u32 = 146;
const DEPOT_DIRTY: u32 = 147;
const DEPOT_WINDOW: u32 = 148;
const VIEW_WINDOW: u32 = 149;
const TRAIN_LIST: u32 = 150;
const HIDE_FILL: u32 = 151;
const COUNT_CHAIN: u32 = 152;
const DEPOT_TRACK: u32 = 153;
const TICKS_LEAVE_DEPOT: u32 = 154;
const UPDATE_DELTA: u32 = 155;
const BASE_VIEWPORT: u32 = 156;
const SHOW_EFFECT: u32 = 157;
const ADVANCE_DISTANCE: u32 = 158;
const ORDER_FREE: u32 = 159;
const HANDLE_BREAKDOWN: u32 = 160;
const LOST_WARN: u32 = 161;
const LOCAL_COMPANY: u32 = 162;
const STUCK_NEWS: u32 = 163;
const SET_NEXT: u32 = 164;
const CRASH_GROUND: u32 = 165;
const CRASH_EVENT: u32 = 166;
const CRASH_NEWS: u32 = 167;
const CRASH_RATING: u32 = 168;
const DISASTER_SOUND: u32 = 169;
const CRASH_SOUND: u32 = 170;
const LARGE_EXPLOSION: u32 = 171;
const SMALL_EXPLOSION: u32 = 172;
const VISIT_TYPE: u32 = 173;
const WRITE_VISIT_TYPE: u32 = 174;
const TRAIN_VISIT: u32 = 175;
const ARRIVAL_NEWS: u32 = 176;
const DISCONNECT: u32 = 177;
const ENTER_TILE: u32 = 178;
const ENTER_DEPOT: u32 = 179;
const PROCESS_ORDERS: u32 = 180;
const LOADING: u32 = 181;
const LEAVE_STATION: u32 = 182;
const BEGIN_LOADING: u32 = 183;
const ARRIVAL_TRIGGERS: u32 = 184;
const LEAVE_SOUND: u32 = 185;
const DELETE_VEHICLE: u32 = 186;
const CHOOSE_TRACK: u32 = 187;
const CHECK_NEXT: u32 = 188;
const TRY_PATH: u32 = 189;
const FREE_RESERVATION: u32 = 190;
const CLEAR_RESERVATION: u32 = 191;
const CHECK_REVERSE: u32 = 192;
impl Game {
    fn property(&self, id: u32, prop: u32, default: u64) -> u64 {
        self.leaf(PROPERTY, id, self.op(prop, id), default, 0)
    }
    fn consist_changed(&self, id: u32, allowed: u8) {
        let mut max_speed = u16::MAX;
        let head = self.read(id);
        let mut first_engine = if head.front != 0 {
            head.engine
        } else {
            u16::MAX
        };
        self.write(id, W_TOTAL_LENGTH, 0);
        self.set(id, 3, 0);
        let mut tilt = true;
        let mut min_curve_mod = i16::MAX;
        let mut u = id;
        while u != INVALID {
            let part = self.read(u);
            self.write(
                u,
                W_FIRST_ENGINE,
                u64::from(if u == id { u16::MAX } else { first_engine }),
            );
            self.set(u, 4, self.op(RAIL_TYPES, u));
            if part.engine_part != 0 {
                first_engine = part.engine;
            }
            self.set(u, 8, self.op(USER_DEFAULT, u));
            self.op(INVALIDATE_GRF, id);
            self.op(INVALIDATE_GRF, u);
            u = self.read(u).next;
        }
        u = id;
        while u != INVALID {
            self.set(u, 8, self.property(u, PROP_TRAIN_USER_DATA, self.get(u, 8)));
            self.op(INVALIDATE_GRF, id);
            self.op(INVALIDATE_GRF, u);
            u = self.read(u).next;
        }
        u = id;
        while u != INVALID {
            if self.op(RAIL_TILT, u) == 0 {
                tilt = false;
            }
            min_curve_mod = min_curve_mod.min(self.op(CURVE_MOD, u) as i16);
            self.op(CACHE_OVERRIDE, u);
            self.write(u, W_COLOURMAP, 0);
            self.val(VIS_EFFECT, u, 1);
            let powered = self.op(POW_WAG_POWER, id) != 0
                && self.op(RAILVEH_WAGON, u) != 0
                && self.op(WAGON_OVERRIDE, u) != 0
                && self.read(u).vis_effect & 0x80 == 0;
            self.set_flag(u, 3, powered);
            if self.read(u).articulated == 0 {
                if self.op(ENGINE_POWER, u) > 0 {
                    self.set(
                        id,
                        3,
                        self.get(id, 3) | self.val(ALL_POWERED, u, self.get(u, 4)),
                    );
                }
                if self.flag(u, 6) {
                    self.set(u, 4, self.get(u, 4) | 1);
                    self.set(u, 3, self.get(u, 3) | 1);
                }
                if (self.op(RAILVEH_WAGON, u) == 0 || self.op(WAGON_SPEED_LIMITS, u) != 0)
                    && self.op(WAGON_OVERRIDE, u) == 0
                {
                    let speed =
                        self.property(u, PROP_TRAIN_SPEED, self.op(SPEED_DEFAULT, u)) as u16;
                    if speed != 0 {
                        max_speed = max_speed.min(speed);
                    }
                }
            }
            let cap = self.op(CAPACITY, u) as u16;
            let part = self.read(u);
            if allowed & 2 != 0 {
                if part.cargo_cap > cap {
                    self.val(TRUNCATE_CARGO, u, u64::from(cap));
                }
                self.write(u, W_REFIT_CAP, u64::from(cap.min(self.read(u).refit_cap)));
                self.write(u, W_CARGO_CAP, u64::from(cap));
            } else if cap != part.cargo_cap {
                self.op(CAPACITY_ERROR, u);
            }
            self.write(
                u,
                W_CARGO_AGE,
                self.property(
                    u,
                    PROP_TRAIN_CARGO_AGE_PERIOD,
                    self.op(CARGO_AGE_DEFAULT, u),
                ),
            );
            let mut length = u16::MAX;
            if self.op(GRF_VERSION, u) >= 8 {
                length = self.property(u, PROP_TRAIN_SHORTEN_FACTOR, u64::from(u16::MAX)) as u16;
                if length != u16::MAX && length >= 8 {
                    self.val(LENGTH_ERROR, u, u64::from(length));
                }
            } else if self.op(LENGTH_CALLBACK, u) != 0 {
                length = self.op(CALLBACK_LENGTH, u) as u16;
            }
            if length == u16::MAX {
                length = self.op(LENGTH_DEFAULT, u) as u16;
            }
            length = 8 - length.min(7);
            if allowed & 1 != 0 {
                self.write(u, W_LENGTH, u64::from(length));
            } else if length != self.read(u).length {
                self.op(LENGTH_CHANGED, u);
            }
            self.write(
                id,
                W_TOTAL_LENGTH,
                u64::from(self.read(id).total_length.wrapping_add(self.read(u).length)),
            );
            self.op(INVALIDATE_GRF, id);
            self.op(INVALIDATE_GRF, u);
            u = self.read(u).next;
        }
        self.write(id, W_MAX_SPEED, u64::from(max_speed));
        self.set(id, 7, u64::from(tilt));
        self.set(id, 9, u64::from(min_curve_mod as u16));
        self.set(id, 10, u64::from(self.curve_limit(id)));
        self.op(CARGO_CHANGED, id);
        if self.read(id).front != 0 {
            self.update_acceleration(id);
            self.op(CONSIST_WINDOWS, id);
        }
    }
    fn curve_limit(&self, id: u32) -> u16 {
        let mut max_speed = i32::from(u16::MAX);
        if self.op(ACC_MODEL, id) == 0 {
            return max_speed as u16;
        }
        let mut curves = [0_i32; 2];
        let (mut numcurve, mut sum, mut pos, mut lastpos) = (0_i32, 0_i32, 0_i32, -1_i32);
        let mut u = id;
        loop {
            let part = self.read(u);
            if part.next == INVALID {
                break;
            }
            let diff = part.direction.wrapping_sub(self.read(part.next).direction) & 7;
            if diff != 0 {
                if diff == 7 {
                    curves[0] = curves[0].wrapping_add(1);
                }
                if diff == 1 {
                    curves[1] = curves[1].wrapping_add(1);
                }
                if diff == 7 || diff == 1 {
                    if lastpos != -1 {
                        numcurve = numcurve.wrapping_add(1);
                        sum = sum.wrapping_add(pos.wrapping_sub(lastpos));
                        if pos.wrapping_sub(lastpos) <= 8 && max_speed > 88 {
                            max_speed = 88;
                        }
                    }
                    lastpos = pos;
                }
                if diff == 6 || diff == 2 {
                    max_speed = 61;
                }
            }
            u = part.next;
            pos = pos.wrapping_add(i32::from(self.read(u).length));
        }
        if numcurve > 0 && max_speed > 88 {
            if curves == [1, 1] {
                max_speed = i32::from(u16::MAX);
            } else {
                sum = sum.wrapping_add(7) / 8;
                sum /= numcurve;
                let d = 13 - sum.clamp(1, 12);
                max_speed = 232 - d * d;
            }
        }
        if max_speed != i32::from(u16::MAX) {
            max_speed = max_speed
                .wrapping_add((max_speed / 2).wrapping_mul(self.op(CURVE_ADVANTAGE, id) as i32));
            if self.get(id, 7) != 0 {
                max_speed = max_speed.wrapping_add(max_speed / 5);
            }
            max_speed = max_speed
                .wrapping_add(max_speed.wrapping_mul(i32::from(self.get(id, 9) as i16)) / 256);
            max_speed = max_speed.clamp(2, i32::from(u16::MAX));
        }
        max_speed as u16
    }
    fn stop_location(&self, id: u32, station: u16, tile: u32) -> (i32, i32, i32) {
        let ahead = (self.leaf(PLATFORM_AHEAD, id, u64::from(station), u64::from(tile), 0) as i32)
            .wrapping_mul(16);
        let length = (self.leaf(PLATFORM_LENGTH, id, u64::from(station), u64::from(tile), 0)
            as i32)
            .wrapping_mul(16);
        let v = self.read(id);
        let mut loc = 1;
        if i32::from(v.total_length) >= length {
            loc = 2;
        } else if v.order == 1 && v.order_destination == station {
            loc = self.op(STOP_LOCATION, id);
        }
        let stop = match loc {
            0 => i32::from(v.total_length),
            1 => length.wrapping_sub(length.wrapping_sub(i32::from(v.total_length)) / 2),
            _ => length,
        };
        (
            stop.wrapping_sub((i32::from(v.length) + 1) / 2),
            ahead,
            length,
        )
    }
    fn current_max_speed(&self, id: u32) -> i32 {
        let v = self.read(id);
        let model = self.op(ACC_MODEL, id);
        let mut max_speed = if model == 0 {
            i32::from(v.max_track_speed)
        } else {
            self.get(id, 10) as i32
        };
        if model == 1 && self.val(IS_STATION, id, u64::from(v.tile)) != 0 {
            let station = self.val(STATION, id, u64::from(v.tile)) as u16;
            if self.val(ORDER_STOP, id, u64::from(station)) != 0 {
                let (stop, ahead, length) = self.stop_location(id, station, v.tile);
                let distance = ahead / 16 - length.wrapping_sub(stop) / 16;
                if distance > 0 {
                    let mut limit = 120;
                    let delta = i32::from(v.speed) / distance.wrapping_add(1);
                    if max_speed > i32::from(v.speed) - delta {
                        limit = i32::from(v.speed) - delta / 10;
                    }
                    limit = limit.max(25_i32.wrapping_mul(distance));
                    max_speed = max_speed.min(limit);
                }
            }
        }
        let mut u = id;
        while u != INVALID {
            let part = self.read(u);
            if model == 1 && self.get(u, 5) == u64::from(DEPOT) {
                max_speed = max_speed.min(61);
                break;
            }
            if self.get(u, 5) == u64::from(WORMHOLE) && part.status & 1 == 0 {
                max_speed = max_speed.min(self.val(BRIDGE_SPEED, u, u64::from(part.tile)) as i32);
            }
            u = part.next;
        }
        max_speed = max_speed.min(self.op(ORDER_MAX_SPEED, id) as i32);
        max_speed.min(i32::from(self.read(id).max_track_speed))
    }
    fn update_acceleration(&self, id: u32) {
        let v = self.read(id);
        let accel = (v.power / v.weight).wrapping_mul(4).clamp(1, 255);
        self.write(id, W_ACCELERATION, u64::from(accel));
    }
    fn update_speed(&self, id: u32) -> i32 {
        let v = self.read(id);
        let braking = v.status & 2 != 0 || self.flag(id, 0) || self.flag(id, 8);
        let max = self.current_max_speed(id);
        if self.op(ACC_MODEL, id) == 0 {
            self.leaf(
                UPDATE_SPEED,
                id,
                i64::from(i32::from(v.acceleration) * if braking { -4 } else { 2 }) as u64,
                0,
                max as u64,
            ) as i32
        } else {
            self.leaf(
                UPDATE_SPEED,
                id,
                self.op(ACCELERATION, id),
                u64::from(!braking) * 2,
                max as u64,
            ) as i32
        }
    }
    fn mark_dirty(&self, id: u32) {
        let mut u = id;
        loop {
            self.write(u, W_COLOURMAP, 0);
            self.leaf(VIEWPORT, u, 1, 0, 0);
            u = self.read(u).next;
            if u == INVALID {
                break;
            }
        }
        self.op(CARGO_CHANGED, id);
        self.update_acceleration(id);
    }
    fn trackdir(&self, id: u32) -> u8 {
        let v = self.read(id);
        if v.status & 128 != 0 {
            return u8::MAX;
        }
        let track = self.get(id, 5) as u8;
        if track == DEPOT {
            return self.val(
                DIAG_TRACKDIR,
                id,
                self.val(DEPOT_DIR, id, u64::from(v.tile)),
            ) as u8;
        }
        if track == WORMHOLE {
            return self.val(
                DIAG_TRACKDIR,
                id,
                self.val(DIR_DIAG, id, u64::from(v.direction)),
            ) as u8;
        }
        self.leaf(
            TRACK_DIRECTION,
            id,
            self.val(FIRST_TRACK, id, u64::from(track)),
            u64::from(v.direction),
            0,
        ) as u8
    }
    fn running_cost(&self, id: u32) -> i64 {
        let mut cost = 0_i64;
        let mut u = id;
        loop {
            if self.op(COST_CLASS, u) != self.op(INVALID_PRICE, u) {
                let mut factor =
                    self.property(u, PROP_TRAIN_RUNNING_COST_FACTOR, self.op(COST_DEFAULT, u))
                        as u32;
                if factor != 0 {
                    if self.read(u).multiheaded != 0 {
                        factor /= 2;
                    }
                    cost = cost.saturating_add(self.val(PRICE, u, u64::from(factor)) as i64);
                }
            }
            u = self.read(u).next_unit;
            if u == INVALID {
                break;
            }
        }
        cost
    }
}
use std::cell::Cell;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context, Poll, Waker};
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Action {
    pub op: u32,
    pub id: u32,
    pub a: u64,
    pub b: u64,
    pub c: u64,
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
pub struct Task {
    future: Pin<Box<dyn Future<Output = u64>>>,
    mailbox: Rc<Mailbox>,
}
const BREAKDOWN_SPEEDS: [u16; 16] = [
    225, 210, 195, 180, 165, 150, 135, 120, 105, 90, 75, 60, 45, 30, 15, 15,
];
impl Game {
    async fn action(&self, op: u32, id: u32, a: u64, b: u64, c: u64) -> u64 {
        Reentry {
            mailbox: self.mailbox.clone(),
            action: Action { op, id, a, b, c },
            yielded: false,
        }
        .await
    }
    fn count(&self, index: u64) {
        self.val(PROFILE, INVALID, index);
    }
    fn nearby(&self, mode: u32, tile: u32, x: i32, y: i32) -> Vec<u32> {
        let count = (self.leaves.nearby)(mode, tile, x, y, std::ptr::null_mut(), 0);
        let mut ids = vec![0; count];
        (self.leaves.nearby)(mode, tile, x, y, ids.as_mut_ptr(), count);
        ids
    }
    fn write_status(&self, id: u32, mask: u8, value: bool) {
        let old = self.read(id).status;
        self.write(
            id,
            W_STATUS,
            u64::from(if value { old | mask } else { old & !mask }),
        );
    }
    fn track(&self, id: u32) -> u8 {
        self.get(id, 5) as u8
    }
    fn wait_inc(&self, id: u32) -> u16 {
        let next = (self.get(id, 2) as u16).wrapping_add(1);
        self.set(id, 2, u64::from(next));
        next
    }
    fn tile_op(&self, op: u32, tile: u32) -> u64 {
        self.val(op, INVALID, u64::from(tile))
    }
    fn tile_diag(&self, tile: u32, dir: u8) -> u32 {
        self.leaf(TILE_ADD_DIAG, INVALID, u64::from(tile), u64::from(dir), 0) as u32
    }
    fn exitdir(&self, id: u32) -> u8 {
        self.leaf(
            VEH_EXIT_DIR,
            id,
            u64::from(self.read(id).direction),
            u64::from(self.track(id)),
            0,
        ) as u8
    }
    fn compatible(&self, id: u32, tile: u32) -> bool {
        self.val(COMPATIBLE_RAIL_OWNER, id, u64::from(tile)) != 0
            && (self.read(id).front == 0
                || self.get(id, 3) & (1 << self.tile_op(RAIL_TYPE, tile)) != 0)
    }
    fn can_leave(&self, id: u32) -> bool {
        let v = self.read(id);
        let track = self.track(id);
        if track == WORMHOLE || track == DEPOT {
            return false;
        }
        if self.tile_op(IS_TUNNELBRIDGE, v.tile) != 0
            && self.tile_op(TUNNEL_DIR, v.tile) as u8 * 2 + 1 == v.direction
        {
            return false;
        }
        if self.tile_op(IS_DEPOT, v.tile) != 0
            && (self.tile_op(DEPOT_DIR, v.tile) as u8 ^ 2) * 2 + 1 == v.direction
        {
            return false;
        }
        true
    }
    fn approaching_crossing(&self, id: u32) -> u32 {
        if !self.can_leave(id) {
            return INVALID;
        }
        let v = self.read(id);
        let dir = self.exitdir(id);
        let tile = v
            .tile
            .wrapping_add(self.val(TILE_OFFSET_DIAG, id, u64::from(dir)) as u32);
        if self.tile_op(IS_CROSSING, tile) == 0
            || self.val(DIAG_AXIS, id, u64::from(dir)) == self.tile_op(CROSSING_ROAD_AXIS, tile)
            || !self.compatible(id, tile)
        {
            return INVALID;
        }
        tile
    }
    fn train_on_tile(&self, tile: u32) -> bool {
        !self.nearby(0, tile, 0, 0).is_empty()
    }
    fn crossing_approach(&self, tile: u32) -> bool {
        let dir = self.val(AXIS_DIAG, INVALID, self.tile_op(CROSSING_RAIL_AXIS, tile)) as u8;
        for d in [dir, dir ^ 2] {
            let from = tile.wrapping_add(self.val(TILE_OFFSET_DIAG, INVALID, u64::from(d)) as u32);
            for id in self.nearby(0, from, 0, 0) {
                let v = self.read(id);
                if v.status & 128 == 0 && v.front != 0 && self.approaching_crossing(id) == tile {
                    return true;
                }
            }
        }
        false
    }
    fn check_crossing(&self, tile: u32) -> bool {
        self.tile_op(CROSSING_RESERVED, tile) != 0
            || self.train_on_tile(tile)
            || self.crossing_approach(tile)
    }
    fn update_crossing_tile(&self, tile: u32, sound: bool, force: bool) {
        let barred = force || self.check_crossing(tile);
        if barred != (self.tile_op(CROSSING_BARRED, tile) != 0) {
            self.count(if barred { 13 } else { 14 });
            if barred && sound && self.op(AMBIENT_SOUND, INVALID) != 0 {
                self.tile_op(CROSSING_SOUND, tile);
            }
            self.leaf(
                WRITE_CROSSING_BAR,
                INVALID,
                u64::from(tile),
                u64::from(barred),
                0,
            );
            self.tile_op(DIRTY_TILE, tile);
        }
    }
    fn update_crossing(&self, tile: u32, sound: bool, force: bool) {
        if self.tile_op(IS_CROSSING, tile) == 0 {
            return;
        }
        let mut state = force;
        let axis = self.tile_op(CROSSING_ROAD_AXIS, tile);
        let dir = self.val(AXIS_DIAG, INVALID, axis) as u8;
        for d in [dir, dir ^ 2] {
            let mut t = tile;
            while !state
                && u64::from(t) < self.op(MAP_SIZE, INVALID)
                && self.tile_op(IS_CROSSING, t) != 0
                && self.tile_op(CROSSING_ROAD_AXIS, t) == axis
            {
                state |= self.check_crossing(t);
                t = self.tile_diag(t, d);
            }
        }
        self.update_crossing_tile(tile, sound, state);
        for d in [dir, dir ^ 2] {
            let mut t = self.tile_diag(tile, d);
            while u64::from(t) < self.op(MAP_SIZE, INVALID)
                && self.tile_op(IS_CROSSING, t) != 0
                && self.tile_op(CROSSING_ROAD_AXIS, t) == axis
            {
                self.update_crossing_tile(t, sound, state);
                t = self.tile_diag(t, d);
            }
        }
    }
    fn adjacent_crossing_dirty(&self, tile: u32, axis: u64) {
        let dir = self.val(AXIS_DIAG, INVALID, axis) as u8;
        for d in [dir, dir ^ 2] {
            let t = self.tile_diag(tile, d);
            if u64::from(t) < self.op(MAP_SIZE, INVALID)
                && self.tile_op(IS_CROSSING, t) != 0
                && self.tile_op(CROSSING_ROAD_AXIS, t) == axis
            {
                self.tile_op(DIRTY_TILE, t);
            }
        }
    }
    fn crossing_removed(&self, tile: u32, axis: u64) {
        let dir = self.val(AXIS_DIAG, INVALID, axis) as u8;
        for d in [dir, dir ^ 2] {
            let diff = self.val(TILE_OFFSET_DIAG, INVALID, u64::from(d)) as u32;
            let mut occupied = false;
            let mut t = tile.wrapping_add(diff);
            while u64::from(t) < self.op(MAP_SIZE, INVALID)
                && self.tile_op(IS_CROSSING, t) != 0
                && self.tile_op(CROSSING_ROAD_AXIS, t) == axis
            {
                occupied |= self.check_crossing(t);
                t = t.wrapping_add(diff);
            }
            if occupied {
                t = tile.wrapping_add(diff);
                if u64::from(t) < self.op(MAP_SIZE, INVALID)
                    && self.tile_op(IS_CROSSING, t) != 0
                    && self.tile_op(CROSSING_ROAD_AXIS, t) == axis
                {
                    self.tile_op(DIRTY_TILE, t);
                }
            } else {
                t = tile.wrapping_add(diff);
                while u64::from(t) < self.op(MAP_SIZE, INVALID)
                    && self.tile_op(IS_CROSSING, t) != 0
                    && self.tile_op(CROSSING_ROAD_AXIS, t) == axis
                {
                    if self.tile_op(CROSSING_BARRED, t) != 0 {
                        self.leaf(WRITE_CROSSING_BAR, INVALID, u64::from(t), 0, 0);
                        self.tile_op(DIRTY_TILE, t);
                    } else {
                        self.tile_op(DIRTY_TILE, t);
                        break;
                    }
                    t = t.wrapping_add(diff);
                }
            }
        }
    }
    fn bar_crossing(&self, tile: u32) {
        if self.tile_op(CROSSING_BARRED, tile) == 0 {
            self.leaf(WRITE_CROSSING_RES, INVALID, u64::from(tile), 1, 0);
            self.update_crossing(tile, true, false);
        }
    }
    fn mark_stuck(&self, id: u32) {
        if !self.flag(id, 8) {
            self.set_flag(id, 8, true);
            self.set(id, 2, 0);
            self.write(id, W_SPEED, 0);
            self.write(id, W_SUBSPEED, 0);
            self.op(LAST_SPEED, id);
            self.op(START_STOP_DIRTY, id);
        }
    }
    fn whole_in_depot(&self, id: u32) -> bool {
        let tile = self.read(id).tile;
        let mut u = id;
        while u != INVALID {
            if self.track(u) != DEPOT || self.read(u).tile != tile {
                return false;
            }
            u = self.read(u).next;
        }
        true
    }
    fn next_offset(&self, id: u32) -> i32 {
        let v = self.read(id);
        i32::from(v.length) / 2
            + if v.next == INVALID {
                0
            } else {
                (i32::from(self.read(v.next).length) + 1) / 2
            }
    }
    async fn after_swap(&self, id: u32) {
        if self.track(id) == WORMHOLE {
            self.count(3);
        }
        if self.track(id) != DEPOT {
            self.write(id, W_DIRECTION, u64::from(self.read(id).direction ^ 4));
        }
        let v = self.read(id);
        if self.track(id) != WORMHOLE {
            self.action(ENTER_TILE, id, u64::from(v.tile), v.x as u64, v.y as u64)
                .await;
        } else {
            let vt = self.leaf(TILE_VIRT, id, v.x as u64, v.y as u64, 0) as u32;
            if self.tile_op(IS_TUNNELBRIDGE, vt) != 0 {
                self.action(ENTER_TILE, id, u64::from(vt), v.x as u64, v.y as u64)
                    .await;
                if self.track(id) != WORMHOLE && self.tile_op(IS_BRIDGE, self.read(id).tile) != 0 {
                    self.op(POSITION, id);
                    self.leaf(INCLINATION, id, 1, 1, 0);
                    return;
                }
            }
        }
        self.op(POSITION, id);
        self.leaf(VIEWPORT, id, 1, 1, 0);
    }
    fn swap_ground_flags(&self, a: u32, b: u32) {
        let fa = self.read(a).gv_flags;
        let fb = self.read(b).gv_flags;
        self.write(a, W_GV_FLAGS, u64::from(fa & !3));
        self.write(b, W_GV_FLAGS, u64::from(self.read(b).gv_flags & !3));
        if fa & 1 != 0 {
            self.write(b, W_GV_FLAGS, u64::from(self.read(b).gv_flags | 2));
        } else if fa & 2 != 0 {
            self.write(b, W_GV_FLAGS, u64::from(self.read(b).gv_flags | 1));
        }
        if fb & 1 != 0 {
            self.write(a, W_GV_FLAGS, u64::from(self.read(a).gv_flags | 2));
        } else if fb & 2 != 0 {
            self.write(a, W_GV_FLAGS, u64::from(self.read(a).gv_flags | 1));
        }
    }
    async fn reverse_swap(&self, id: u32, l: i32, r: i32) {
        let (mut a, mut b) = (id, id);
        for _ in 0..l {
            a = self.read(a).next;
        }
        for _ in 0..r {
            b = self.read(b).next;
        }
        if a != b {
            let va = self.read(a);
            let vb = self.read(b);
            self.write_status(b, 1, va.status & 1 != 0);
            self.write_status(a, 1, vb.status & 1 != 0);
            let ta = self.track(a);
            self.set(a, 5, u64::from(self.track(b)));
            self.set(b, 5, u64::from(ta));
            for (field, x, y) in [
                (
                    W_DIRECTION,
                    u64::from(va.direction),
                    u64::from(vb.direction),
                ),
                (W_X, va.x as u64, vb.x as u64),
                (W_Y, va.y as u64, vb.y as u64),
                (W_TILE, u64::from(va.tile), u64::from(vb.tile)),
                (W_Z, va.z as u64, vb.z as u64),
            ] {
                self.write(a, field, y);
                self.write(b, field, x);
            }
            self.swap_ground_flags(a, b);
            self.after_swap(a).await;
            self.after_swap(b).await;
        } else {
            self.swap_ground_flags(a, a);
            self.after_swap(a).await;
        }
    }
    async fn advance_before_swap(&self, id: u32) {
        let (mut base, mut first, mut last) = (id, id, self.read(id).last);
        let mut length = self.op(COUNT_CHAIN, id) as u32;
        while length > 2 {
            last = self.read(last).previous;
            first = self.read(first).next;
            let diff = self.next_offset(base) - self.next_offset(last);
            for _ in 0..diff {
                self.count(4);
                Box::pin(self.controller(first, self.read(last).next, true)).await;
            }
            base = first;
            length -= 2;
        }
    }
    async fn advance_after_swap(&self, id: u32) {
        let mut dep = id;
        while self.read(dep).next != INVALID
            && (self.track(dep) == DEPOT || self.track(self.read(dep).next) != DEPOT)
        {
            dep = self.read(dep).next;
        }
        let leave = self.read(dep).next;
        if leave != INVALID {
            let d = self.op(TICKS_LEAVE_DEPOT, dep) as i32;
            if d <= 0 {
                self.write_status(leave, 1, false);
                self.set(
                    leave,
                    5,
                    1 << self.tile_op(DEPOT_TRACK, self.read(leave).tile),
                );
                for _ in d..=0 {
                    Box::pin(self.controller(leave, INVALID, true)).await;
                }
            }
        } else {
            dep = INVALID;
        }
        let (mut base, mut first, mut last) = (id, id, self.read(id).last);
        let mut length = self.op(COUNT_CHAIN, id) as u32;
        let mut nomove = dep == INVALID;
        while length > 2 {
            if base == dep {
                break;
            }
            if last == dep {
                nomove = true;
            }
            last = self.read(last).previous;
            first = self.read(first).next;
            let diff = self.next_offset(last) - self.next_offset(base);
            for _ in 0..diff {
                self.count(5);
                Box::pin(self.controller(
                    first,
                    if nomove {
                        self.read(last).next
                    } else {
                        INVALID
                    },
                    true,
                ))
                .await;
            }
            base = first;
            length -= 2;
        }
    }
    async fn reverse(&self, id: u32) {
        self.count(2);
        if self.tile_op(IS_DEPOT, self.read(id).tile) != 0 {
            if self.whole_in_depot(id) {
                return;
            }
            self.op(DEPOT_DIRTY, id);
        }
        if !self.flag(id, 8) {
            self.op(FREE_RESERVATION, id);
        }
        let crossing = self.approaching_crossing(id);
        let mut r = self.op(COUNT_CHAIN, id) as i32 - 1;
        self.advance_before_swap(id).await;
        let mut l = 0;
        loop {
            self.reverse_swap(id, l, r).await;
            l += 1;
            r -= 1;
            if l > r {
                break;
            }
        }
        self.advance_after_swap(id).await;
        if self.tile_op(IS_DEPOT, self.read(id).tile) != 0 {
            self.op(DEPOT_DIRTY, id);
        }
        self.set_flag(id, 7, !self.flag(id, 7));
        self.set_flag(id, 0, false);
        self.consist_changed(id, 0);
        let mut u = id;
        while u != INVALID {
            self.leaf(VIEWPORT, u, 0, 0, 0);
            u = self.read(u).next;
        }
        if crossing != INVALID {
            self.update_crossing(crossing, false, false);
        }
        let crossing = self.approaching_crossing(id);
        if crossing != INVALID {
            self.bar_crossing(crossing);
        }
        if self.track(id) == DEPOT {
            if self.flag(id, 8) {
                self.op(START_STOP_DIRTY, id);
            }
            self.set_flag(id, 8, false);
            return;
        }
        let v = self.read(id);
        let mut dir = self.exitdir(id);
        if self.tile_op(IS_DEPOT, v.tile) != 0 || self.tile_op(IS_TUNNELBRIDGE, v.tile) != 0 {
            dir = u8::MAX;
        }
        if self.leaf(SIGNALS_UPDATE, id, u64::from(v.tile), u64::from(dir), 0)
            == self.op(SIGSEG_PBS, id)
            || self.op(RESERVE_PATHS, id) != 0
        {
            let td = self.trackdir(id);
            let mut okay = !(self.tile_op(IS_RAILWAY, v.tile) != 0
                && self.leaf(HAS_SIGNAL_TD, id, u64::from(v.tile), u64::from(td), 0) != 0
                && self.val(
                    SIGNAL_PBS,
                    id,
                    self.leaf(
                        SIGNAL_TYPE,
                        id,
                        u64::from(v.tile),
                        u64::from(self.track(id).trailing_zeros()),
                        0,
                    ),
                ) == 0);
            if self.tile_op(IS_DEPOT, v.tile) != 0
                && self.val(TRACKDIR_EXIT, id, u64::from(td)) == self.tile_op(DEPOT_DIR, v.tile)
            {
                okay = false;
            }
            if self.tile_op(IS_STATION, v.tile) != 0 {
                self.leaf(
                    SET_PLATFORM_RES,
                    id,
                    u64::from(v.tile),
                    self.val(TRACKDIR_EXIT, id, u64::from(td)),
                    1,
                );
            }
            if self.action(TRY_PATH, id, 0, u64::from(okay), 0).await != 0 {
                self.action(CHECK_NEXT, id, 0, 0, 0).await;
            } else if self.read(id).order != 3 {
                self.mark_stuck(id);
            }
        } else if self.flag(id, 8) {
            self.set_flag(id, 8, false);
            self.set(id, 2, 0);
        }
    }
    async fn approaching_end(&self, id: u32, signal: bool, reverse: bool) -> bool {
        let v = self.read(id);
        let mut x = v.x as u32 & 15;
        let y = v.y as u32 & 15;
        match v.direction {
            0 => x = (!x).wrapping_add(!y).wrapping_add(25),
            7 => x = (!y).wrapping_add(16),
            1 => x = (!x).wrapping_add(16),
            2 => x = (!x).wrapping_add(y).wrapping_add(9),
            3 => x = y,
            4 => x = x.wrapping_add(y).wrapping_sub(7),
            6 => x = (!y).wrapping_add(x).wrapping_add(9),
            _ => {}
        }
        if !signal
            && x.wrapping_add(
                u32::from(v.length + 1) / 2 * if v.direction & 1 != 0 { 1 } else { 2 },
            ) >= 16
        {
            self.write(id, W_SPEED, 0);
            if reverse {
                Box::pin(self.reverse(id)).await;
            }
            return false;
        }
        self.write_status(id, 16, true);
        let speed = BREAKDOWN_SPEEDS[(x & 15) as usize];
        if speed < v.speed {
            self.write(id, W_SPEED, u64::from(speed));
        }
        true
    }
    async fn line_ends(&self, id: u32, reverse: bool) -> bool {
        let v = self.read(id);
        let t = v.breakdown;
        if t > 1 {
            self.write_status(id, 16, true);
            let speed = BREAKDOWN_SPEEDS[usize::from((!t >> 4) & 15)];
            if speed < v.speed {
                self.write(id, W_SPEED, u64::from(speed));
            }
        } else {
            self.write_status(id, 16, false);
        }
        if !self.can_leave(id) {
            return true;
        }
        let dir = self.exitdir(id);
        let tile = v
            .tile
            .wrapping_add(self.val(TILE_OFFSET_DIAG, id, u64::from(dir)) as u32);
        let ts = self.leaf(TRACK_STATUS, id, u64::from(tile), u64::from(dir ^ 2), 0) as u32;
        let reaches = self.val(TRACKDIR_REACHES, id, u64::from(dir)) as u16;
        let tds = ts as u16 & reaches;
        let reds = (ts >> 16) as u16 & reaches;
        let mut bits = ((tds | tds >> 8) & 63) as u8;
        if self.leaf(
            NO_90,
            id,
            self.tile_op(TILE_RAIL_TYPE, v.tile),
            self.tile_op(TILE_RAIL_TYPE, tile),
            0,
        ) != 0
        {
            bits &= !(self.val(
                TRACK_CROSSES,
                id,
                u64::from(self.track(id).trailing_zeros()),
            ) as u8);
        }
        if bits == 0 || !self.compatible(id, tile) {
            return self.approaching_end(id, false, reverse).await;
        }
        if tds & reds != 0 {
            return self.approaching_end(id, true, reverse).await;
        }
        if self.tile_op(IS_CROSSING, tile) != 0 {
            self.bar_crossing(tile);
        }
        true
    }
}
const INITIAL_SUBCOORD: [[[u8; 3]; 4]; 6] = [
    [[15, 8, 1], [0, 0, 0], [0, 8, 5], [0, 0, 0]],
    [[0, 0, 0], [8, 0, 3], [0, 0, 0], [8, 15, 7]],
    [[0, 0, 0], [7, 0, 2], [0, 7, 6], [0, 0, 0]],
    [[15, 8, 2], [0, 0, 0], [0, 0, 0], [8, 15, 6]],
    [[15, 7, 0], [8, 0, 4], [0, 0, 0], [0, 0, 0]],
    [[0, 0, 0], [0, 0, 0], [0, 8, 4], [7, 15, 0]],
];
impl Game {
    async fn enter_station(&self, id: u32, station: u16) {
        self.write(id, W_LAST_STATION, u64::from(station));
        if self.val(VISIT_TYPE, id, u64::from(station)) & self.op(TRAIN_VISIT, id) == 0 {
            self.val(WRITE_VISIT_TYPE, id, u64::from(station));
            self.val(ARRIVAL_NEWS, id, u64::from(station));
        }
        self.set(id, 6, 0);
        self.op(VIEW_WINDOW, id);
        self.action(BEGIN_LOADING, id, 0, 0, 0).await;
        self.action(ARRIVAL_TRIGGERS, id, u64::from(station), 0, 0)
            .await;
    }
    fn speed_z(&self, id: u32, old: i32) {
        let v = self.read(id);
        if old == v.z || self.op(ACC_MODEL, id) != 0 {
            return;
        }
        if old < v.z {
            self.write(
                id,
                W_SPEED,
                u64::from(v.speed - ((u32::from(v.speed) * 64) >> 8) as u16),
            );
        } else {
            let speed = v.speed.wrapping_add(2);
            if speed <= v.max_track_speed {
                self.write(id, W_SPEED, u64::from(speed));
            }
        }
    }
    fn moved_signals(&self, id: u32, tile: u32, dir: u8) -> bool {
        if self.tile_op(IS_RAILWAY, tile) != 0 && self.tile_op(HAS_SIGNALS, tile) != 0 {
            let tracks = self.tile_op(TRACK_BITS, tile) as u16;
            let tds =
                (tracks | tracks << 8) & self.val(TRACKDIR_REACHES, id, u64::from(dir)) as u16;
            let td = tds.trailing_zeros() as u8;
            let exit = self.val(TRACKDIR_EXIT, id, u64::from(td));
            // Tile owner is used here, rather than the moving train's owner.
            if self.leaf(
                SIGNALS_UPDATE_OWNER,
                id,
                u64::from(tile),
                exit,
                self.tile_op(TILE_OWNER, tile),
            ) == self.op(SIGSEG_PBS, id)
                && self.leaf(HAS_SIGNAL_TD, id, u64::from(tile), u64::from(td), 0) != 0
            {
                if self.val(
                    SIGNAL_PBS,
                    id,
                    self.leaf(SIGNAL_TYPE, id, u64::from(tile), u64::from(td & 7), 0),
                ) == 0
                {
                    return true;
                }
            }
        }
        false
    }
    // Result: 0 moved, 1 no move/early stop, 2 invalid rail, 3 requested reversal.
    async fn move_vehicle(&self, id: u32, prev: u32, reverse: bool) -> (u8, bool) {
        let v = self.read(id);
        if v.articulated != 0 {
            self.count(18);
        }
        let delta = [
            (-1, -1),
            (-1, 0),
            (-1, 1),
            (0, 1),
            (1, 1),
            (1, 0),
            (1, -1),
            (0, -1),
        ];
        let (dx, dy) = delta[usize::from(v.direction)];
        let (mut x, mut y) = (v.x.wrapping_add(dx), v.y.wrapping_add(dy));
        let mut old = v.tile;
        let new = self.leaf(TILE_VIRT, id, x as u64, y as u64, 0) as u32;
        let mut entered = 0_u8;
        let mut signals = false;
        let mut changed = false;
        if self.track(id) != WORMHOLE {
            if old == new {
                if self.track(id) == DEPOT {
                    x = v.x;
                    y = v.y;
                } else {
                    if v.front != 0 && !self.line_ends(id, reverse).await {
                        return (1, false);
                    }
                    let vets = self
                        .action(ENTER_TILE, id, u64::from(new), x as u64, y as u64)
                        .await as u8;
                    if vets & 4 != 0 {
                        return (2, false);
                    }
                    if vets & 1 != 0 {
                        self.enter_station(id, self.tile_op(STATION, new) as u16)
                            .await;
                    }
                }
            } else {
                entered = self.leaf(DIAG_BETWEEN, id, u64::from(old), u64::from(new), 0) as u8;
                let ts =
                    self.leaf(TRACK_STATUS, id, u64::from(new), u64::from(entered ^ 2), 0) as u32;
                let reaches = self.val(TRACKDIR_REACHES, id, u64::from(entered)) as u16;
                let tds = ts as u16 & reaches;
                let reds = (((ts >> 16) as u16 & reaches) | (((ts >> 16) as u16 & reaches) >> 8))
                    as u8
                    & 63;
                let mut bits = ((tds | tds >> 8) & 63) as u8;
                if self.leaf(
                    NO_90,
                    id,
                    self.tile_op(TILE_RAIL_TYPE, old),
                    self.tile_op(TILE_RAIL_TYPE, new),
                    0,
                ) != 0
                    && prev == INVALID
                {
                    bits &= !(self.val(
                        TRACK_CROSSES,
                        id,
                        u64::from(self.track(id).trailing_zeros()),
                    ) as u8);
                }
                if bits == 0 || !self.compatible(id, new) {
                    return (2, false);
                }
                let mut chosen;
                if prev == INVALID {
                    chosen = 1_u8
                        << self
                            .action(
                                CHOOSE_TRACK,
                                id,
                                u64::from(new),
                                u64::from(entered),
                                u64::from(bits),
                            )
                            .await;
                    if self.get(id, 6) != 0
                        && self.tile_op(IS_PLAIN_RAIL, new) != 0
                        && self.tile_op(HAS_SIGNALS, new) != 0
                    {
                        let td = tds.trailing_zeros() as u8;
                        if self.leaf(HAS_SIGNAL_TD, id, u64::from(new), u64::from(td), 0) != 0
                            || (self.leaf(HAS_SIGNAL_TD, id, u64::from(new), u64::from(td ^ 8), 0)
                                != 0
                                && self.leaf(SIGNAL_TYPE, id, u64::from(new), u64::from(td & 7), 0)
                                    != self.op(PBS_SIGNAL_TYPE, id))
                        {
                            self.count(10);
                            self.set(id, 6, u64::from(self.get(id, 6) == 2));
                            self.op(VIEW_WINDOW, id);
                        }
                    }
                    if reds & chosen != 0 && self.get(id, 6) == 0 {
                        let td = tds.trailing_zeros() as u8;
                        if self.flag(id, 8) {
                            return (1, false);
                        }
                        if self.leaf(HAS_SIGNAL_TD, id, u64::from(new), u64::from(td ^ 8), 0) == 0 {
                            self.write(id, W_SPEED, 0);
                            self.write(id, W_SUBSPEED, 0);
                            self.write(id, W_PROGRESS, 255);
                            self.count(8);
                            if self.op(REVERSE_AT_SIGNALS, id) == 0
                                || u64::from(self.wait_inc(id))
                                    < self.op(WAIT_ONEWAY, id) * self.op(DAY_TICKS, id) * 2
                            {
                                return (1, false);
                            }
                        } else if self.leaf(HAS_SIGNAL_TD, id, u64::from(new), u64::from(td), 0)
                            != 0
                        {
                            self.write(id, W_SPEED, 0);
                            self.write(id, W_SUBSPEED, 0);
                            self.write(id, W_PROGRESS, 255);
                            self.count(9);
                            if self.op(REVERSE_AT_SIGNALS, id) == 0
                                || u64::from(self.wait_inc(id))
                                    < self.op(WAIT_TWOWAY, id) * self.op(DAY_TICKS, id) * 2
                            {
                                let dir = self.val(TRACKDIR_EXIT, id, u64::from(td)) as u8;
                                let tile = self.tile_diag(new, dir);
                                let mut waiting = false;
                                for u in self.nearby(0, tile, 0, 0) {
                                    let other = self.read(u);
                                    if other.status & 128 == 0
                                        && other.front != 0
                                        && self.track(u) & 63 != 0
                                        && other.speed <= 5
                                        && self.exitdir(u) == dir ^ 2
                                    {
                                        waiting = true;
                                        break;
                                    }
                                }
                                if !waiting {
                                    return (1, false);
                                }
                            }
                        }
                        if self.op(REVERSE_AT_SIGNALS, id) == 0
                            && self.leaf(ONEWAY_BLOCKING, id, u64::from(new), u64::from(td), 0) == 0
                            && self.leaf(
                                SIGNALS_UPDATE,
                                id,
                                u64::from(self.read(id).tile),
                                u64::from(entered),
                                0,
                            ) == self.op(SIGSEG_PBS, id)
                        {
                            self.set(id, 2, 0);
                            return (1, false);
                        }
                        return (3, false);
                    }
                    self.leaf(
                        TRY_RESERVE,
                        id,
                        u64::from(new),
                        u64::from(chosen.trailing_zeros()),
                        0,
                    );
                } else {
                    let before = self.read(prev);
                    if before.tile == new {
                        chosen = if self.track(prev) == WORMHOLE {
                            bits
                        } else {
                            self.track(prev)
                        };
                    } else {
                        let exit =
                            self.leaf(DIAG_BETWEEN, id, u64::from(new), u64::from(before.tile), 0)
                                as usize;
                        let connecting =
                            [[1, 8, 0, 16], [4, 2, 16, 0], [0, 32, 1, 4], [32, 0, 8, 2]];
                        chosen = connecting[usize::from(entered)][exit];
                    }
                    chosen &= bits;
                }
                let sub = INITIAL_SUBCOORD[chosen.trailing_zeros() as usize][usize::from(entered)];
                x = (x & !15) | i32::from(sub[0]);
                y = (y & !15) | i32::from(sub[1]);
                let dir = sub[2];
                let vets = self
                    .action(ENTER_TILE, id, u64::from(new), x as u64, y as u64)
                    .await as u8;
                if vets & 4 != 0 {
                    return (2, false);
                }
                if vets & 2 == 0 {
                    if self.read(id).front != 0 {
                        self.count(19);
                    }
                    let td = self.leaf(
                        TRACK_DIRECTION,
                        id,
                        u64::from(chosen.trailing_zeros()),
                        u64::from(dir),
                        0,
                    ) as u8;
                    if self.read(id).front != 0
                        && self.leaf(SIGNAL_HAS_PBS, id, u64::from(new), u64::from(td), 0) != 0
                    {
                        self.leaf(SET_SIGNAL_STATE, id, u64::from(new), u64::from(td), 0);
                        self.tile_op(DIRTY_TILE, new);
                    }
                    if self.read(id).next == INVALID {
                        self.leaf(
                            CLEAR_RESERVATION,
                            id,
                            u64::from(self.read(id).tile),
                            u64::from(self.trackdir(id)),
                            0,
                        );
                    }
                    self.write(id, W_TILE, u64::from(new));
                    if self.tile_op(TILE_RAIL_TYPE, new) != self.tile_op(TILE_RAIL_TYPE, old) {
                        self.consist_changed(self.read(id).first, 0);
                    }
                    self.set(id, 5, u64::from(chosen));
                }
                signals = true;
                if dir != self.read(id).direction {
                    if prev == INVALID && self.op(ACC_MODEL, id) == 0 {
                        let diff = self.read(id).direction.wrapping_sub(dir) & 7;
                        let small = if self.op(ACC_TYPE, id) == 2 { 0 } else { 64 };
                        let fraction = if diff == 1 || diff == 7 { small } else { 128 };
                        let speed = self.read(id).speed;
                        self.write(
                            id,
                            W_SPEED,
                            u64::from(speed - ((u32::from(speed) * fraction) >> 8) as u16),
                        );
                    }
                    changed = true;
                    self.write(id, W_DIRECTION, u64::from(dir));
                }
                if self.read(id).front != 0 {
                    self.set(id, 2, 0);
                    let crossing = self.approaching_crossing(id);
                    if crossing != INVALID
                        && self.tile_op(CROSSING_RESERVED, crossing) != 0
                        && self.op(AMBIENT_SOUND, id) != 0
                    {
                        self.tile_op(CROSSING_SOUND, crossing);
                    }
                    self.action(CHECK_NEXT, id, 0, 0, 0).await;
                }
                if vets & 1 != 0 {
                    self.enter_station(id, self.tile_op(STATION, new) as u16)
                        .await;
                }
            }
        } else {
            if self.tile_op(IS_TUNNELBRIDGE, new) != 0
                && self
                    .action(ENTER_TILE, id, u64::from(new), x as u64, y as u64)
                    .await
                    & 2
                    != 0
            {
                if self.read(id).front != 0 {
                    self.count(20);
                    self.reserve_track(id, new, self.tile_op(TUNNEL_DIR, new) & 1)
                        .await;
                    self.action(CHECK_NEXT, id, 0, 0, 0).await;
                }
                if old == new {
                    old = self.tile_op(OTHER_END, old) as u32;
                }
            } else {
                self.write(id, W_X, x as u64);
                self.write(id, W_Y, y as u64);
                self.op(POSITION, id);
                if self.read(id).status & 1 == 0 {
                    self.val(BASE_VIEWPORT, id, 1);
                }
                return (0, false);
            }
        }
        self.op(UPDATE_DELTA, id);
        self.write(id, W_X, x as u64);
        self.write(id, W_Y, y as u64);
        self.op(POSITION, id);
        let old_z = self.leaf(INCLINATION, id, u64::from(new != old), 0, 0) as i32;
        if prev == INVALID {
            self.speed_z(id, old_z);
        }
        if signals {
            if self.read(id).front != 0 && self.moved_signals(id, new, entered) {
                if (self.leaf(
                    HAS_RESERVED,
                    id,
                    u64::from(new),
                    u64::from(self.track(id)),
                    0,
                ) == 0
                    && self
                        .reserve_track(id, new, u64::from(self.track(id).trailing_zeros()))
                        .await
                        == 0)
                    || self.action(TRY_PATH, id, 0, 0, 0).await == 0
                {
                    self.mark_stuck(id);
                }
            }
            if self.read(id).next == INVALID {
                self.moved_signals(id, old, entered ^ 2);
                if self.tile_op(IS_CROSSING, old) != 0 {
                    self.update_crossing(old, false, false);
                }
            }
        }
        if self.read(id).front != 0 && u64::from(self.read(id).tick) % self.op(BACKOFF, id) == 0 {
            self.action(CHECK_NEXT, id, 0, 0, 0).await;
        }
        (0, changed)
    }
    async fn controller(&self, mut id: u32, nomove: u32, reverse: bool) -> bool {
        let first = self.read(id).first;
        let mut prev = self.read(id).previous;
        let mut changed = false;
        while id != nomove {
            let (outcome, direction) = self.move_vehicle(id, prev, reverse).await;
            changed |= direction;
            if outcome == 1 {
                return false;
            }
            if outcome == 2 && prev != INVALID {
                self.op(DISCONNECT, id);
            }
            if outcome >= 2 {
                if reverse {
                    self.set(id, 2, 0);
                    self.write(id, W_SPEED, 0);
                    self.write(id, W_SUBSPEED, 0);
                    Box::pin(self.reverse(id)).await;
                }
                return false;
            }
            prev = id;
            id = self.read(id).next;
        }
        if changed {
            self.set(first, 10, u64::from(self.curve_limit(first)));
        }
        true
    }
}

const TILE_OWNER: u32 = 194;

const PBS_SIGNAL_TYPE: u32 = 195;

const TILE_OFFSET_AXIS: u32 = 196;

const REVERSE_SINGLE_BLOCKED: u32 = 197;

const STOPPED_IN_DEPOT: u32 = 198;

const REVERSE_WINDOWS: u32 = 199;

const RESERVE_UNDER: u32 = 200;

const FIND_DEPOT: u32 = 201;

const RESERVE_TRACK: u32 = 202;
impl Game {
    async fn reserve_track(&self, id: u32, tile: u32, track: u64) -> u64 {
        if self.tile_op(IS_STATION_RAIL, tile) != 0 {
            // The original station reservation invokes randomisation and animation.
            self.action(RESERVE_TRACK, id, u64::from(tile), track, 1)
                .await
        } else {
            self.leaf(TRY_RESERVE, id, u64::from(tile), track, 1)
        }
    }
    fn crash(&self, id: u32, flooded: bool) -> u32 {
        let mut victims = 0_u32;
        if self.read(id).front != 0 {
            victims += 2;
            if !self.flag(id, 8) {
                self.op(FREE_RESERVATION, id);
            }
            let mut u = id;
            while u != INVALID {
                let v = self.read(u);
                self.leaf(
                    CLEAR_RESERVATION,
                    u,
                    u64::from(v.tile),
                    u64::from(self.trackdir(u)),
                    0,
                );
                if self.tile_op(IS_TUNNELBRIDGE, v.tile) != 0 {
                    self.leaf(SET_TUNNEL_RES, u, self.tile_op(OTHER_END, v.tile), 0, 0);
                }
                u = self.read(u).next;
            }
            let crossing = self.approaching_crossing(id);
            if crossing != INVALID {
                self.update_crossing(crossing, false, false);
            }
            self.op(HIDE_FILL, id);
        }
        victims = victims.wrapping_add(self.val(CRASH_GROUND, id, u64::from(flooded)) as u32);
        self.set(id, 1, if flooded { 4000 } else { 1 });
        victims
    }
    async fn crashed(&self, id: u32) -> u32 {
        let mut victims = 0;
        if self.read(id).status & 128 == 0 {
            victims = self.crash(id, false);
            self.val(CRASH_EVENT, id, u64::from(victims));
        }
        self.action(RESERVE_UNDER, id, 0, 0, 0).await;
        victims
    }
    async fn collision_one(&self, other: u32, id: u32) -> u32 {
        if self.track(other) == DEPOT {
            return 0;
        }
        let v = self.read(other);
        let t = self.read(id);
        if v.owner != t.owner || v.first == id {
            return 0;
        }
        let x = v.x.wrapping_sub(t.x);
        let y = v.y.wrapping_sub(t.y);
        if ((y.wrapping_add(7) | x.wrapping_add(7)) as u32) & !15 != 0 {
            return 0;
        }
        let min = (i32::from(v.length) + 1) / 2 + (i32::from(t.length) + 1) / 2 - 1;
        if x.wrapping_mul(x).wrapping_add(y.wrapping_mul(y)) > min.wrapping_mul(min)
            || v.z.wrapping_sub(t.z).wrapping_abs() > 5
        {
            return 0;
        }
        let victims = self.crashed(id).await;
        victims.wrapping_add(self.crashed(v.first).await)
    }
    async fn collision(&self, id: u32) -> bool {
        if self.track(id) == DEPOT {
            return false;
        }
        let v = self.read(id);
        let mut victims = 0_u32;
        if self.track(id) == WORMHOLE {
            for u in self.nearby(0, v.tile, 0, 0) {
                victims = victims.wrapping_add(self.collision_one(u, id).await);
            }
            for u in self.nearby(0, self.tile_op(OTHER_END, v.tile) as u32, 0, 0) {
                victims = victims.wrapping_add(self.collision_one(u, id).await);
            }
        } else {
            for u in self.nearby(1, 0, v.x, v.y) {
                victims = victims.wrapping_add(self.collision_one(u, id).await);
            }
        }
        if victims == 0 {
            return false;
        }
        self.count(15);
        self.val(CRASH_NEWS, id, u64::from(victims));
        self.op(CRASH_RATING, id);
        if self.op(DISASTER_SOUND, id) != 0 {
            self.op(CRASH_SOUND, id);
        }
        true
    }
    fn platform_occupied(&self, tile: u32) -> bool {
        let delta = self.val(TILE_OFFSET_AXIS, INVALID, self.tile_op(STATION_AXIS, tile)) as u32;
        let mut t = tile;
        while self.leaf(
            STATION_COMPATIBLE,
            INVALID,
            u64::from(t),
            u64::from(tile),
            0,
        ) != 0
        {
            if self.train_on_tile(t) {
                return true;
            }
            t = t.wrapping_sub(delta);
        }
        t = tile.wrapping_add(delta);
        while self.leaf(
            STATION_COMPATIBLE,
            INVALID,
            u64::from(t),
            u64::from(tile),
            0,
        ) != 0
        {
            if self.train_on_tile(t) {
                return true;
            }
            t = t.wrapping_add(delta);
        }
        false
    }
    async fn delete_last(&self, mut id: u32) {
        self.count(16);
        let first = self.read(id).first;
        let mut last = id;
        while self.read(id).next != INVALID {
            last = id;
            id = self.read(id).next;
        }
        self.val(SET_NEXT, last, u64::from(INVALID));
        if first != id {
            self.consist_changed(first, 3);
            if self.track(first) == DEPOT {
                self.val(DEPOT_WINDOW, first, u64::from(self.read(first).tile));
            }
            self.write(id, W_LAST_STATION, u64::from(self.read(first).last_station));
        }
        let mut tracks = self.track(id);
        let v = self.read(id);
        let tile = v.tile;
        let owner = v.owner;
        self.action(DELETE_VEHICLE, id, 0, 0, 0).await;
        if tracks == WORMHOLE {
            tracks = 1 << (self.tile_op(TUNNEL_DIR, tile) & 1);
        }
        let track = tracks.trailing_zeros() as u8;
        if self.leaf(HAS_RESERVED, INVALID, u64::from(tile), u64::from(tracks), 0) != 0 {
            self.leaf(UNRESERVE, INVALID, u64::from(tile), u64::from(track), 0);
            let mut remaining = 0_u8;
            for u in self.nearby(0, tile, 0, 0) {
                if self.read(u).status & 128 == 0 {
                    continue;
                }
                let t = self.track(u);
                if t == WORMHOLE {
                    remaining |= 1 << (self.tile_op(TUNNEL_DIR, self.read(u).tile) & 1);
                } else if t != DEPOT {
                    remaining |= t;
                }
            }
            for t in 0..6 {
                if remaining & (1 << t) != 0 {
                    self.reserve_track(INVALID, tile, t).await;
                }
            }
        }
        if self.tile_op(IS_CROSSING, tile) != 0 {
            self.update_crossing(tile, false, false);
        }
        if self.tile_op(IS_STATION, tile) != 0 {
            let occupied = self.platform_occupied(tile);
            let dir = self.val(AXIS_DIAG, INVALID, self.tile_op(STATION_AXIS, tile)) as u8;
            self.leaf(
                SET_PLATFORM_RES,
                INVALID,
                u64::from(tile),
                u64::from(dir),
                u64::from(occupied),
            );
            self.leaf(
                SET_PLATFORM_RES,
                INVALID,
                u64::from(tile),
                u64::from(dir ^ 2),
                u64::from(occupied),
            );
        }
        if self.tile_op(IS_TUNNELBRIDGE, tile) != 0 || self.tile_op(IS_DEPOT, tile) != 0 {
            self.leaf(
                SIGNALS_UPDATE_OWNER,
                INVALID,
                u64::from(tile),
                u64::from(u8::MAX),
                u64::from(owner),
            );
        } else {
            self.leaf(
                SIGNALS_BOTH,
                INVALID,
                u64::from(tile),
                u64::from(track),
                u64::from(owner),
            );
        }
    }
    fn change_dir_randomly(&self, mut id: u32) {
        loop {
            if self.read(id).status & 1 == 0 {
                let delta = [7, 0, 0, 1][(self.services.random() & 3) as usize];
                self.write(
                    id,
                    W_DIRECTION,
                    u64::from(self.read(id).direction.wrapping_add(delta) & 7),
                );
                if self.track(id) != WORMHOLE {
                    self.op(POSITION, id);
                    self.leaf(INCLINATION, id, 0, 1, 0);
                } else {
                    self.leaf(VIEWPORT, id, 0, 1, 0);
                }
            }
            id = self.read(id).next;
            if id == INVALID {
                break;
            }
        }
    }
    async fn handle_crashed(&self, id: u32) -> bool {
        let state = (self.get(id, 1) as u16).wrapping_add(1);
        self.set(id, 1, u64::from(state));
        if state == 4 && self.read(id).status & 1 == 0 {
            self.op(LARGE_EXPLOSION, id);
        }
        if state <= 200 {
            let mut r = self.services.random();
            if crate::services::chance16_i(1, 7, r) {
                let mut index = (r.wrapping_mul(10) >> 16) as i32;
                let mut u = id;
                loop {
                    index -= 1;
                    if index < 0 {
                        r = self.services.random();
                        self.leaf(
                            SMALL_EXPLOSION,
                            u,
                            u64::from(((r >> 8) & 7) + 2),
                            u64::from(((r >> 16) & 7) + 2),
                            u64::from((r & 7) + 5),
                        );
                        break;
                    }
                    u = self.read(u).next;
                    if u == INVALID {
                        break;
                    }
                }
            }
        }
        let tick = self.read(id).tick;
        if state <= 240 && tick & 3 == 0 {
            self.change_dir_randomly(id);
        }
        if state >= 4440 && tick & 31 == 0 {
            let remains = self.read(id).next != INVALID;
            self.delete_last(id).await;
            return remains;
        }
        true
    }
    async fn stay_depot(&self, id: u32) -> bool {
        if !self.whole_in_depot(id) {
            return false;
        }
        let v = self.read(id);
        if v.power == 0 {
            self.write_status(id, 2, true);
            self.val(DEPOT_WINDOW, id, u64::from(v.tile));
            return true;
        }
        if self.op(WAIT_UNBUNCH, id) != 0 {
            return true;
        }
        if self.get(id, 6) == 0 {
            if self.wait_inc(id) < 37 {
                self.op(TRAIN_LIST, id);
                return true;
            }
            self.set(id, 2, 0);
        }
        let segment = if self.op(RESERVE_PATHS, id) != 0 {
            self.op(SIGSEG_PBS, id)
        } else {
            self.leaf(SIGNALS_UPDATE, id, u64::from(v.tile), u64::from(u8::MAX), 0)
        };
        if self.get(id, 6) == 0
            && (segment == self.op(SIGSEG_FULL, id) || self.tile_op(HAS_DEPOT_RES, v.tile) != 0)
        {
            self.op(TRAIN_LIST, id);
            return true;
        }
        if self.read(id).order == 2 && self.read(id).tile == self.read(id).dest {
            if self.tile_op(HAS_DEPOT_RES, self.read(id).tile) == 0 {
                self.count(21);
                self.action(ENTER_DEPOT, id, 0, 0, 0).await;
            }
            return true;
        }
        if segment == self.op(SIGSEG_PBS, id)
            && self.action(TRY_PATH, id, 0, 0, 0).await == 0
            && self.get(id, 6) == 0
        {
            self.op(TRAIN_LIST, id);
            self.mark_stuck(id);
            return true;
        }
        self.leaf(SET_DEPOT_RES, id, u64::from(self.read(id).tile), 1, 0);
        if self.op(SHOW_RESERVATION, id) != 0 {
            self.tile_op(DIRTY_TILE, self.read(id).tile);
        }
        self.count(6);
        self.op(SERVICE, id);
        self.op(LEAVE_UNBUNCH, id);
        self.action(LEAVE_SOUND, id, 0, 0, 0).await;
        self.op(TRAIN_LIST, id);
        self.set(
            id,
            5,
            if self.read(id).direction & 2 != 0 {
                2
            } else {
                1
            },
        );
        self.write_status(id, 1, false);
        self.write(id, W_SPEED, 0);
        self.leaf(VIEWPORT, id, 1, 1, 0);
        self.op(POSITION, id);
        self.leaf(
            SIGNALS_UPDATE,
            id,
            u64::from(self.read(id).tile),
            u64::from(u8::MAX),
            0,
        );
        self.update_acceleration(id);
        self.op(DEPOT_DIRTY, id);
        false
    }
    async fn loco(&self, id: u32, mode: bool) -> bool {
        self.count(u64::from(mode));
        if self.read(id).status & 128 != 0 {
            return if mode {
                true
            } else {
                self.handle_crashed(id).await
            };
        }
        if self.get(id, 6) != 0 {
            self.set_flag(id, 8, false);
            self.op(START_STOP_DIRTY, id);
        }
        if self.op(HANDLE_BREAKDOWN, id) != 0 {
            return true;
        }
        if self.flag(id, 0) && self.read(id).speed == 0 {
            self.reverse(id).await;
        }
        let v = self.read(id);
        if v.status & 2 != 0 && v.speed == 0 {
            return true;
        }
        let valid_order = v.order != 0 && v.order != 7;
        if self.action(PROCESS_ORDERS, id, 0, 0, 0).await != 0 && self.op(CHECK_REVERSE, id) != 0 {
            self.set(id, 2, 0);
            self.write(id, W_SPEED, 0);
            self.write(id, W_SUBSPEED, 0);
            self.set_flag(id, 9, false);
            self.reverse(id).await;
            return true;
        } else if self.flag(id, 9) {
            let v = self.read(id);
            let mut dir = self.exitdir(id);
            if self.tile_op(IS_DEPOT, v.tile) != 0 || self.tile_op(IS_TUNNELBRIDGE, v.tile) != 0 {
                dir = u8::MAX;
            }
            if self.leaf(SIGNALS_UPDATE, id, u64::from(v.tile), u64::from(dir), 0)
                == self.op(SIGSEG_PBS, id)
                || self.op(RESERVE_PATHS, id) != 0
            {
                self.action(TRY_PATH, id, 1, 1, 0).await;
            }
            self.set_flag(id, 9, false);
        }
        self.action(LOADING, id, u64::from(mode), 0, 0).await;
        if self.read(id).order == 3 || self.stay_depot(id).await {
            return true;
        }
        if !mode {
            self.op(SHOW_EFFECT, id);
        }
        if !valid_order && self.read(id).order != 0 {
            self.action(CHECK_NEXT, id, 0, 0, 0).await;
        }
        if !mode && self.flag(id, 8) {
            self.count(11);
            let wait = u64::from(self.wait_inc(id));
            let turn = wait % (self.op(WAIT_PBS, id) * self.op(DAY_TICKS, id)) == 0
                && self.op(REVERSE_AT_SIGNALS, id) != 0;
            if !turn && wait % self.op(BACKOFF, id) != 0 && self.get(id, 6) == 0 {
                return true;
            }
            if self.action(TRY_PATH, id, 0, 0, 0).await == 0 {
                if turn {
                    self.count(12);
                    self.reverse(id).await;
                }
                if self.flag(id, 8)
                    && self.get(id, 2) > 2 * self.op(WAIT_PBS, id) * self.op(DAY_TICKS, id)
                {
                    if self.op(LOST_WARN, id) != 0
                        && u64::from(self.read(id).owner) == self.op(LOCAL_COMPANY, id)
                    {
                        self.op(STUCK_NEWS, id);
                    }
                    self.set(id, 2, 0);
                }
                if self.get(id, 6) == 0 {
                    return true;
                }
                self.set_flag(id, 8, false);
                self.set(id, 2, 0);
                self.op(START_STOP_DIRTY, id);
            }
        }
        if self.read(id).order == 4 {
            self.op(ORDER_FREE, id);
            self.op(START_STOP_DIRTY, id);
            return true;
        }
        let mut distance = self.update_speed(id);
        if self.read(id).speed == 0 && self.read(id).status & 2 != 0 {
            self.set(id, 6, 0);
            self.op(VIEW_WINDOW, id);
        }
        let mut advance = self.op(ADVANCE_DISTANCE, id) as i32;
        if distance < advance {
            if self.read(id).speed == 0 {
                self.op(LAST_SPEED, id);
            }
        } else {
            self.line_ends(id, true).await;
            loop {
                distance = distance.wrapping_sub(advance);
                self.controller(id, INVALID, true).await;
                if self.collision(id).await {
                    break;
                }
                advance = self.op(ADVANCE_DISTANCE, id) as i32;
                if distance < advance || self.read(id).speed == 0 {
                    break;
                }
                let v = self.read(id);
                if (v.order == 6 || v.order == 1)
                    && v.nonstop & 2 != 0
                    && self.tile_op(IS_STATION_ANY, v.tile) != 0
                    && u64::from(v.order_destination) == self.tile_op(STATION, v.tile)
                {
                    self.action(PROCESS_ORDERS, id, 0, 0, 0).await;
                }
            }
            self.op(LAST_SPEED, id);
        }
        let mut u = id;
        while u != INVALID {
            if self.read(u).status & 1 == 0 {
                self.leaf(VIEWPORT, u, 0, 0, 0);
            }
            u = self.read(u).next;
        }
        if self.read(id).progress == 0 {
            self.write(id, W_PROGRESS, distance as u64);
        }
        true
    }
    async fn tick(&self, id: u32) -> bool {
        self.write(id, W_TICK, u64::from(self.read(id).tick.wrapping_add(1)));
        let v = self.read(id);
        if v.front != 0 {
            if v.status & 2 == 0 || v.speed > 0 {
                self.write(id, W_RUNNING, u64::from(v.running.wrapping_add(1)));
            }
            self.write(
                id,
                W_ORDER_TIME,
                self.read(id).order_time.wrapping_add(1) as u64,
            );
            if !self.loco(id, false).await {
                return false;
            }
            return self.loco(id, true).await;
        } else if v.free_wagon != 0 && v.status & 128 != 0 {
            let state = (self.get(id, 1) as u16).wrapping_add(1);
            self.set(id, 1, u64::from(state));
            if state >= 4400 {
                self.count(17);
                self.action(DELETE_VEHICLE, id, 0, 0, 0).await;
                return false;
            }
        }
        true
    }
    fn needs_service(&self, id: u32) {
        if self.op(SERVINT, id) == 0 || self.op(NEEDS_SERVICE, id) == 0 {
            return;
        }
        if self.op(CHAIN_DEPOT, id) != 0 {
            self.count(7);
            self.op(SERVICE, id);
            return;
        }
        let max = self.op(MAX_DEPOT_PENALTY, id) as u32;
        let depot = self.val(FIND_DEPOT, id, u64::from(max));
        let length = (depot >> 32) as u32;
        if length == u32::MAX || length > max {
            if self.read(id).order == 2 {
                self.op(ORDER_DUMMY, id);
                self.op(START_STOP_DIRTY, id);
            }
            return;
        }
        let tile = depot as u32;
        let depot_id = self.tile_op(DEPOT_INDEX, tile) as u16;
        let v = self.read(id);
        if v.order == 2
            && v.order_destination != depot_id
            && !crate::services::chance16_i(3, 16, self.services.random())
        {
            return;
        }
        self.op(SUPPRESS_IMPLICIT, id);
        self.val(ORDER_DEPOT_SERVICE, id, u64::from(depot_id));
        self.write(id, W_DEST, u64::from(tile));
        self.op(START_STOP_DIRTY, id);
    }
    fn economy_day(&self, id: u32) {
        self.op(ECONOMY_AGE, id);
        let day = self.read(id).day.wrapping_add(1);
        self.write(id, W_DAY, u64::from(day));
        if day & 7 == 0 {
            self.op(DECREASE_VALUE, id);
        }
        if self.read(id).front != 0 {
            self.op(CHECK_BREAKDOWN, id);
            self.needs_service(id);
            self.op(CHECK_ORDERS, id);
            if self.read(id).order == 1 {
                let tile = self.op(STATION_DEST, id) as u32;
                if tile != INVALID {
                    self.write(id, W_DEST, u64::from(tile));
                }
            }
            let running = self.read(id).running;
            if running != 0 {
                let cost = self.running_cost(id).saturating_mul(i64::from(running))
                    / (self.op(COST_DIVISOR, id) as i64);
                self.val(PAY_RUNNING, id, cost as u64);
                self.op(RUNNING_WINDOWS, id);
            }
        }
    }
}
impl Game {
    fn next_force(&self, id: u32) -> u8 {
        let v = self.read(id);
        if v.status & 128 != 0 || self.get(id, 6) == 2 {
            return 0;
        }
        if !self.flag(id, 8) {
            return if self.op(CHAIN_DEPOT, id) != 0 { 1 } else { 2 };
        }
        let dir = self.val(TRACKDIR_EXIT, id, u64::from(self.trackdir(id))) as u8;
        let tile = self.tile_diag(v.tile, dir);
        if tile == INVALID
            || self.tile_op(IS_RAILWAY, tile) == 0
            || self.tile_op(HAS_SIGNALS, tile) == 0
        {
            return 1;
        }
        let tracks = self.val(DIAG_REACHES_TRACKS, id, u64::from(dir)) as u8
            & self.tile_op(TRACK_BITS, tile) as u8;
        if tracks != 0
            && self.leaf(
                HAS_SIGNAL,
                id,
                u64::from(tile),
                u64::from(tracks.trailing_zeros()),
                0,
            ) != 0
        {
            2
        } else {
            1
        }
    }
    async fn reverse_command(&self, id: u32, execute: bool, single: bool) -> u8 {
        if single {
            if self.read(id).multiheaded != 0 || self.op(REVERSE_SINGLE_BLOCKED, id) != 0 {
                return 1;
            }
            let first = self.read(id).first;
            if self.op(STOPPED_IN_DEPOT, first) == 0 {
                return 2;
            }
            if execute {
                self.set_flag(id, 4, !self.flag(id, 4));
                self.consist_changed(first, 3);
                self.op(REVERSE_WINDOWS, first);
            }
        } else {
            let v = self.read(id);
            if v.front == 0 || v.status & 128 != 0 || v.breakdown != 0 {
                return 3;
            }
            if execute {
                if self.read(id).order == 3 {
                    let last = self.read(self.read(id).last);
                    if self.tile_op(IS_STATION_ANY, last.tile) == 0
                        || self.tile_op(STATION, last.tile)
                            != self.tile_op(STATION, self.read(id).tile)
                    {
                        self.action(LEAVE_STATION, id, 0, 0, 0).await;
                    }
                }
                self.set(id, 6, 0);
                self.op(VIEW_WINDOW, id);
                if self.op(ACC_MODEL, id) != 0 && self.read(id).speed != 0 {
                    self.set_flag(id, 0, !self.flag(id, 0));
                } else {
                    self.write(id, W_SPEED, 0);
                    self.op(LAST_SPEED, id);
                    self.op(HIDE_FILL, id);
                    self.reverse(id).await;
                }
                self.op(RESET_UNBUNCH, id);
            }
        }
        0
    }
    fn force_command(&self, id: u32, execute: bool) -> bool {
        if self.read(id).front == 0 {
            return false;
        }
        if execute {
            self.set(id, 6, u64::from(self.next_force(id)));
            self.op(VIEW_WINDOW, id);
            self.op(RESET_UNBUNCH, id);
        }
        true
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_create(
    kind: u32,
    id: u32,
    a: u64,
    b: u64,
    c: u64,
    leaves: *const Leaves,
    services: *const Services,
) -> *mut Task {
    let mailbox = Rc::new(Mailbox::default());
    // SAFETY: Immutable descriptors live throughout invocation; copy unaligned on MSVC x86.
    let g = Game {
        leaves: unsafe { leaves.read_unaligned() },
        services: unsafe { services.read_unaligned() },
        mailbox: mailbox.clone(),
    };
    let future = Box::pin(async move {
        match kind {
            0 => {
                g.consist_changed(id, a as u8);
                0
            }
            1 => u64::from(g.curve_limit(id)),
            2 => g.current_max_speed(id) as u64,
            3 => {
                g.update_acceleration(id);
                0
            }
            4 => g.update_speed(id) as u64,
            5 => {
                g.mark_dirty(id);
                0
            }
            6 => u64::from(g.tick(id).await),
            7 => {
                g.op(AGE, id);
                0
            }
            8 => {
                g.economy_day(id);
                0
            }
            9 => g.running_cost(id) as u64,
            10 => u64::from(g.trackdir(id)),
            11 => {
                g.mark_stuck(id);
                0
            }
            12 => u64::from(g.approaching_crossing(id)),
            13 => {
                g.reverse_swap(id, a as i32, b as i32).await;
                0
            }
            14 => u64::from(g.train_on_tile(a as u32)),
            15 => {
                g.update_crossing(a as u32, b != 0, c != 0);
                0
            }
            16 => {
                g.adjacent_crossing_dirty(a as u32, b);
                0
            }
            17 => {
                g.crossing_removed(a as u32, b);
                0
            }
            18 => {
                g.reverse(id).await;
                0
            }
            19 => u64::from(g.controller(id, a as u32, b != 0).await),
            20 => u64::from(g.crash(id, a != 0)),
            21 => u64::from(g.stay_depot(id).await),
            22 => {
                g.needs_service(id);
                0
            }
            23 => u64::from(g.next_force(id)),
            24 => u64::from(g.reverse_command(id, a != 0, b != 0).await),
            25 => u64::from(g.force_command(id, a != 0)),
            _ => unreachable!(),
        }
    });
    Box::into_raw(Box::new(Task { future, mailbox }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_advance(task: *mut Task, response: u64) -> Action {
    // SAFETY: Sole serialized task advance; pending releases every owner/world borrow.
    let task = unsafe { &mut *task };
    task.mailbox.response.set(response);
    let waker = Waker::noop();
    let mut context = Context::from_waker(waker);
    match task.future.as_mut().poll(&mut context) {
        Poll::Ready(value) => Action {
            op: u32::MAX,
            a: value,
            ..Action::default()
        },
        Poll::Pending => task.mailbox.action.get(),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_destroy(task: *mut Task) {
    // SAFETY: C++ owns one task and cancels/destroys once, including callback exceptions.
    unsafe {
        drop(Box::from_raw(task));
    }
}

const IS_STATION_ANY: u32 = 203;

const PROFILE: u32 = 204;

const IS_STATION_RAIL: u32 = 205;
