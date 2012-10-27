// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller_impl.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {

SessionStateControllerImpl::TestApi::TestApi(
    SessionStateControllerImpl* controller)
    : controller_(controller) {
}

SessionStateControllerImpl::TestApi::~TestApi() {
}

SessionStateControllerImpl::SessionStateControllerImpl()
    : login_status_(user::LOGGED_IN_NONE),
      system_is_locked_(false),
      shutting_down_(false),
      shutdown_after_lock_(false) {
  Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
}

SessionStateControllerImpl::~SessionStateControllerImpl() {
  Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
}

void SessionStateControllerImpl::OnLoginStateChanged(user::LoginStatus status) {
  if (status != user::LOGGED_IN_LOCKED)
    login_status_ = status;
  system_is_locked_ = (status == user::LOGGED_IN_LOCKED);
}

void SessionStateControllerImpl::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  if (!shutting_down_) {
    shutting_down_ = true;
    Shell* shell = ash::Shell::GetInstance();
    shell->env_filter()->set_cursor_hidden_by_filter(false);
    shell->cursor_manager()->ShowCursor(false);
    animator_->ShowBlackLayer();
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_HIDE);
  }
}

void SessionStateControllerImpl::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (IsLocked()) == locked)
    return;

  system_is_locked_ = locked;

  if (locked) {
    animator_->StartAnimation(
        internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        internal::SessionStateAnimator::ANIMATION_FADE_IN);
    lock_timer_.Stop();
    lock_fail_timer_.Stop();

    if (shutdown_after_lock_) {
      shutdown_after_lock_ = false;
      StartLockToShutdownTimer();
    }
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND |
        internal::SessionStateAnimator::LAUNCHER |
        internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        internal::SessionStateAnimator::ANIMATION_RESTORE);
    animator_->DropBlackLayer();
  }
}

void SessionStateControllerImpl::OnStartingLock() {
  if (shutting_down_ || system_is_locked_)
    return;

  // Ensure that the black layer is visible -- if the screen was locked via
  // the wrench menu, we won't have already shown the black background
  // as part of the slow-close animation.
  animator_->ShowBlackLayer();

  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER,
      internal::SessionStateAnimator::ANIMATION_HIDE);

  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_FULL_CLOSE);

  // Hide the screen locker containers so we can make them fade in later.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_HIDE);
}

void SessionStateControllerImpl::StartLockAnimationAndLockImmediately() {
  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE);
  OnLockTimeout();
}

void SessionStateControllerImpl::StartLockAnimation(bool shutdown_after_lock) {
  shutdown_after_lock_ = shutdown_after_lock;

  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE);
  StartLockTimer();
}

void SessionStateControllerImpl::StartShutdownAnimation() {
  animator_->ShowBlackLayer();
  animator_->StartAnimation(
      internal::SessionStateAnimator::kAllContainersMask,
      internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE);

  StartPreShutdownAnimationTimer();
}

bool SessionStateControllerImpl::IsEligibleForLock() {
  return IsLoggedInAsNonGuest() && !IsLocked() && !LockRequested();
}

bool SessionStateControllerImpl::IsLocked() {
  return system_is_locked_;
}

bool SessionStateControllerImpl::LockRequested() {
  return lock_fail_timer_.IsRunning();
}

bool SessionStateControllerImpl::ShutdownRequested() {
  return shutting_down_;
}

bool SessionStateControllerImpl::CanCancelLockAnimation() {
  return lock_timer_.IsRunning();
}

void SessionStateControllerImpl::CancelLockAnimation() {
  if (!CanCancelLockAnimation())
    return;
  shutdown_after_lock_ = false;
  animator_->StartAnimation(
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE);
  animator_->ScheduleDropBlackLayer();
  lock_timer_.Stop();
}

bool SessionStateControllerImpl::CanCancelShutdownAnimation() {
  return pre_shutdown_timer_.IsRunning() ||
         shutdown_after_lock_ ||
         lock_to_shutdown_timer_.IsRunning();
}

void SessionStateControllerImpl::CancelShutdownAnimation() {
  if (!CanCancelShutdownAnimation())
    return;
  if (lock_to_shutdown_timer_.IsRunning()) {
    lock_to_shutdown_timer_.Stop();
    return;
  }
  if (shutdown_after_lock_) {
    shutdown_after_lock_ = false;
    return;
  }

  if (system_is_locked_) {
    // If we've already started shutdown transition at lock screen
    // desktop background needs to be restored immediately.
    animator_->StartAnimation(
        internal::SessionStateAnimator::DESKTOP_BACKGROUND,
        internal::SessionStateAnimator::ANIMATION_RESTORE);
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllLockScreenContainersMask,
        internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE);
    animator_->ScheduleDropBlackLayer();
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE);
    animator_->ScheduleDropBlackLayer();
  }
  pre_shutdown_timer_.Stop();
}

void SessionStateControllerImpl::RequestShutdown() {
  if (!shutting_down_)
    animator_->ShowBlackLayer();
    RequestShutdownImpl();
}

void SessionStateControllerImpl::RequestShutdownImpl() {
  DCHECK(!shutting_down_);
  shutting_down_ = true;

  Shell* shell = ash::Shell::GetInstance();
  shell->env_filter()->set_cursor_hidden_by_filter(false);
  shell->cursor_manager()->ShowCursor(false);

  animator_->ShowBlackLayer();
  if (login_status_ != user::LOGGED_IN_NONE) {
    // Hide the other containers before starting the animation.
    // ANIMATION_FULL_CLOSE will make the screen locker windows partially
    // transparent, and we don't want the other windows to show through.
    animator_->StartAnimation(
        internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
        internal::SessionStateAnimator::LAUNCHER,
        internal::SessionStateAnimator::ANIMATION_HIDE);
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllLockScreenContainersMask,
        internal::SessionStateAnimator::ANIMATION_FULL_CLOSE);
  } else {
    animator_->StartAnimation(
        internal::SessionStateAnimator::kAllContainersMask,
        internal::SessionStateAnimator::ANIMATION_FULL_CLOSE);
  }
  StartRealShutdownTimer();
}

void SessionStateControllerImpl::OnRootWindowHostCloseRequested(
                                                const aura::RootWindow*) {
  if(Shell::GetInstance() && Shell::GetInstance()->delegate())
    Shell::GetInstance()->delegate()->Exit();
}

bool SessionStateControllerImpl::IsLoggedInAsNonGuest() const {
  // TODO(mukai): think about kiosk mode.
  return (login_status_ != user::LOGGED_IN_NONE) &&
         (login_status_ != user::LOGGED_IN_GUEST);
}

void SessionStateControllerImpl::StartLockTimer() {
  lock_timer_.Stop();
  lock_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kSlowCloseAnimMs),
                    this, &SessionStateControllerImpl::OnLockTimeout);
}

void SessionStateControllerImpl::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &SessionStateControllerImpl::OnLockFailTimeout);
}

void SessionStateControllerImpl::OnLockFailTimeout() {
  DCHECK(!system_is_locked_);
  // Undo lock animation.
  animator_->StartAnimation(
      internal::SessionStateAnimator::LAUNCHER |
      internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
      internal::SessionStateAnimator::ANIMATION_RESTORE);
  animator_->DropBlackLayer();
}

void SessionStateControllerImpl::StartLockToShutdownTimer() {
  shutdown_after_lock_ = false;
  lock_to_shutdown_timer_.Stop();
  lock_to_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
      this, &SessionStateControllerImpl::OnLockToShutdownTimeout);
}


void SessionStateControllerImpl::OnLockToShutdownTimeout() {
  DCHECK(system_is_locked_);
  StartShutdownAnimation();
}

void SessionStateControllerImpl::StartPreShutdownAnimationTimer() {
  pre_shutdown_timer_.Stop();
  pre_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
      this, &SessionStateControllerImpl::OnPreShutdownAnimationTimeout);
}

void SessionStateControllerImpl::OnPreShutdownAnimationTimeout() {
  if (!shutting_down_)
    RequestShutdownImpl();
}

void SessionStateControllerImpl::StartRealShutdownTimer() {
  real_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kFastCloseAnimMs +
          kShutdownRequestDelayMs),
      this, &SessionStateControllerImpl::OnRealShutdownTimeout);
}

void SessionStateControllerImpl::OnRealShutdownTimeout() {
  DCHECK(shutting_down_);
#if defined(OS_CHROMEOS)
  if (!base::chromeos::IsRunningOnChromeOS()) {
    ShellDelegate* delegate = Shell::GetInstance()->delegate();
    if (delegate) {
      delegate->Exit();
      return;
    }
  }
#endif
  delegate_->RequestShutdown();
}

void SessionStateControllerImpl::OnLockScreenHide(
    base::Callback<void(void)>& callback) {
  callback.Run();
}

}  // namespace ash
