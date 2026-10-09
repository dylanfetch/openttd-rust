/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Canonical train-private state, owned by one C++ vehicle shell allocation.
//! Accessors copy scalars and retain no Rust reference across shared services.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss
)]

/// Train-private scalars, with exactly the original native widths.
#[derive(Default)]
pub struct State {
    pub(crate) flags: u16,
    pub(crate) crash_anim_pos: u16,
    pub(crate) wait_counter: u16,
    pub(crate) compatible_railtypes: u64,
    pub(crate) railtypes: u64,
    pub(crate) track: u8,
    pub(crate) force_proceed: u8,
    pub(crate) cached_tilt: bool,
    pub(crate) user_def_data: u8,
    pub(crate) cached_curve_speed_mod: i16,
    pub(crate) cached_max_curve_speed: u16,
}

/// Read one copied scalar from a live game-thread owner.
///
/// # Safety
/// `state` must be live and `field` must be a declared selector (0..=10).
#[cfg(test)]
unsafe fn get(state: *const State, field: u8) -> u64 {
    // SAFETY: The caller supplies a live owner; this scalar read ends before return.
    unsafe {
        match field {
            0 => u64::from((*state).flags),
            1 => u64::from((*state).crash_anim_pos),
            2 => u64::from((*state).wait_counter),
            3 => (*state).compatible_railtypes,
            4 => (*state).railtypes,
            5 => u64::from((*state).track),
            6 => u64::from((*state).force_proceed),
            7 => u64::from((*state).cached_tilt),
            8 => u64::from((*state).user_def_data),
            9 => u64::from((*state).cached_curve_speed_mod as u16),
            10 => u64::from((*state).cached_max_curve_speed),
            _ => unreachable!(),
        }
    }
}

/// Write one scalar using the original field width.
///
/// # Safety
/// `state` must be exclusively accessible on the game thread and `field` valid.
#[cfg(test)]
unsafe fn set(state: *mut State, field: u8, value: u64) {
    // SAFETY: Exclusive game-thread access; no retained owner reference.
    unsafe {
        match field {
            0 => (*state).flags = value as u16,
            1 => (*state).crash_anim_pos = value as u16,
            2 => (*state).wait_counter = value as u16,
            3 => (*state).compatible_railtypes = value,
            4 => (*state).railtypes = value,
            5 => (*state).track = value as u8,
            6 => (*state).force_proceed = value as u8,
            7 => (*state).cached_tilt = value != 0,
            8 => (*state).user_def_data = value as u8,
            9 => (*state).cached_curve_speed_mod = value as i16,
            10 => (*state).cached_max_curve_speed = value as u16,
            _ => unreachable!(),
        }
    }
}

/// Allocate one zero-created canonical owner for a vehicle shell.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_train_state_new() -> *mut State {
    Box::into_raw(Box::default())
}

/// Destroy the shell's canonical owner after its pre-destructor.
///
/// # Safety
/// `state` must be a live allocation from `openttd_rust_train_state_new`, destroyed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_destroy(state: *mut State) {
    // SAFETY: The owning shell transfers its allocation exactly once after PreDestructor.
    unsafe {
        drop(Box::from_raw(state));
    }
}

/// Read the shell's canonical `flags` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_flags(state: *const State) -> u16 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).flags }
}
/// Write the shell's canonical `flags` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_flags(state: *mut State, value: u16) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).flags = value;
    }
}
/// Read the shell's canonical `crash_anim_pos` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_crash_anim_pos(state: *const State) -> u16 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).crash_anim_pos }
}
/// Write the shell's canonical `crash_anim_pos` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_crash_anim_pos(
    state: *mut State,
    value: u16,
) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).crash_anim_pos = value;
    }
}
/// Read the shell's canonical `wait_counter` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_wait_counter(state: *const State) -> u16 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).wait_counter }
}
/// Write the shell's canonical `wait_counter` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_wait_counter(state: *mut State, value: u16) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).wait_counter = value;
    }
}
/// Read the shell's canonical `compatible_railtypes` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_compatible_railtypes(
    state: *const State,
) -> u64 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).compatible_railtypes }
}
/// Write the shell's canonical `compatible_railtypes` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_compatible_railtypes(
    state: *mut State,
    value: u64,
) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).compatible_railtypes = value;
    }
}
/// Read the shell's canonical `railtypes` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_railtypes(state: *const State) -> u64 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).railtypes }
}
/// Write the shell's canonical `railtypes` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_railtypes(state: *mut State, value: u64) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).railtypes = value;
    }
}
/// Read the shell's canonical `track` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_track(state: *const State) -> u8 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).track }
}
/// Write the shell's canonical `track` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_track(state: *mut State, value: u8) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).track = value;
    }
}
/// Read the shell's canonical `force_proceed` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_force_proceed(state: *const State) -> u8 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).force_proceed }
}
/// Write the shell's canonical `force_proceed` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_force_proceed(state: *mut State, value: u8) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).force_proceed = value;
    }
}
/// Read the shell's canonical `cached_tilt` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_cached_tilt(state: *const State) -> u8 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { u8::from((*state).cached_tilt) }
}
/// Write the shell's canonical `cached_tilt` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_cached_tilt(state: *mut State, value: u8) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).cached_tilt = value != 0;
    }
}
/// Read the shell's canonical `user_def_data` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_user_def_data(state: *const State) -> u8 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).user_def_data }
}
/// Write the shell's canonical `user_def_data` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_user_def_data(state: *mut State, value: u8) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).user_def_data = value;
    }
}
/// Read the shell's canonical `cached_curve_speed_mod` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_cached_curve_speed_mod(
    state: *const State,
) -> i16 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).cached_curve_speed_mod }
}
/// Write the shell's canonical `cached_curve_speed_mod` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_cached_curve_speed_mod(
    state: *mut State,
    value: i16,
) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).cached_curve_speed_mod = value;
    }
}
/// Read the shell's canonical `cached_max_curve_speed` scalar.
/// # Safety
/// The serialized shell owns a live state; no reference escapes this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_get_cached_max_curve_speed(
    state: *const State,
) -> u16 {
    // SAFETY: Caller supplies a live owner and this scalar read ends before return.
    unsafe { (*state).cached_max_curve_speed }
}
/// Write the shell's canonical `cached_max_curve_speed` scalar.
/// # Safety
/// The serialized shell owns this exclusively accessible live state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_state_set_cached_max_curve_speed(
    state: *mut State,
    value: u16,
) {
    // SAFETY: Exclusive scalar access, retaining no state borrow.
    unsafe {
        (*state).cached_max_curve_speed = value;
    }
}
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn separate_shell_owners_preserve_original_scalar_widths() {
        let first = openttd_rust_train_state_new();
        let second = openttd_rust_train_state_new();
        // SAFETY: Independent live owners, valid selectors, exclusive test access.
        unsafe {
            for field in 0..=10 {
                assert_eq!(get(first, field), 0);
                set(first, field, u64::MAX);
            }
            for field in [0, 1, 2, 9, 10] {
                assert_eq!(get(first, field), u64::from(u16::MAX));
            }
            for field in [3, 4] {
                assert_eq!(get(first, field), u64::MAX);
            }
            for field in [5, 6, 8] {
                assert_eq!(get(first, field), u64::from(u8::MAX));
            }
            assert_eq!(get(first, 7), 1);
            assert_eq!((*first).cached_curve_speed_mod, -1);
            set(first, 9, 0x8000);
            assert_eq!((*first).cached_curve_speed_mod, i16::MIN);
            set(first, 9, 0x1_7fff);
            assert_eq!((*first).cached_curve_speed_mod, i16::MAX);
            for field in 0..=10 {
                assert_eq!(get(second, field), 0);
            }
            openttd_rust_train_state_destroy(first);
            set(second, 2, 0x1_0000);
            assert_eq!(get(second, 2), 0);
            openttd_rust_train_state_destroy(second);
        }
    }
}
