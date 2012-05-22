// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "me2me_preference_pane_confirm_pin.h"

@implementation Me2MePreferencePaneConfirmPin

@synthesize delegate = delegate_;

- (id)init {
    self = [super initWithNibName:@"me2me_preference_pane_confirm_pin"
                           bundle:[NSBundle bundleForClass:[self class]]];
    return self;
}

- (void)dealloc {
  [delegate_ release];
  [super dealloc];
}

- (void)setEmail:(NSString*)email {
  [email_ setStringValue:email];
}

- (void)setButtonText:(NSString*)text {
  [apply_button_ setTitle:text];
}

- (void)setEnabled:(BOOL)enabled {
  [apply_button_ setEnabled:enabled];
  [pin_ setEnabled:enabled];
}

- (void)resetPin {
  [pin_ setStringValue:@""];
}

- (void)onApply:(id)sender {
  [delegate_ applyConfiguration:self
                            pin:[pin_ stringValue]];
}

@end
