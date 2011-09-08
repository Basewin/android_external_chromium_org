// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe user image screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserImageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  UserImageScreen.register = function() {
    var screen = $('user-image');
    UserImageScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
    UserImageScreen.addUserImage(
        'chrome://theme/IDR_BUTTON_USER_IMAGE_TAKE_PHOTO',
        UserImageScreen.handleTakePhoto);
  };

  UserImageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Currently selected user image index (take photo button is with zero
     * index).
     * @type {number}
     */
    selectedUserImage_: -1,

    /** @inheritDoc */
    decorate: function() {
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('userImageScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var okButton = this.ownerDocument.createElement('button');
      okButton.id = 'ok-button';
      okButton.textContent = localStrings.getString('okButtonText');
      okButton.addEventListener('click', function(e) {
        chrome.send('onUserImageAccepted');
      });
      return [ okButton ];
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
    }
  };

  /**
   * Sets user photo as an image for the take photo button and as preview.
   * @param {string} photoUrl Image encoded as data url.
   */
  UserImageScreen.setUserPhoto = function(photoUrl) {
    var takePhotoButton = $('user-image-list').children[0];
    takePhotoButton.src = photoUrl;
    UserImageScreen.selectUserImage(0);
  };

  /**
   * Handler for when the user clicks on "Take photo" button.
   * @param {Event} e Click Event.
   */
  UserImageScreen.handleTakePhoto = function(e) {
    chrome.send('takePhoto');
  };

  /**
   * Handler for when the user clicks on any available user image.
   * @param {Event} e Click Event.
   */
  UserImageScreen.handleImageClick = function(e) {
    chrome.send('selectImage', [e.target.src]);
  };

  /**
   * Appends new image to the end of the image list.
   * @param {string} src A url for the user image.
   * @param {function} clickHandler A handler for click on image.
   */
  UserImageScreen.addUserImage = function(src, clickHandler) {
    var imageElement = document.createElement('img');
    imageElement.src = src;
    imageElement.setAttribute('role', 'option');
    imageElement.setAttribute('tabindex', '-1');
    imageElement.setAttribute(
        'aria-label', src.replace(/(.*\/|\.png)/g, '').replace(/_/g, ' '));
    imageElement.addEventListener('click',
                                  clickHandler,
                                  false);
    $('user-image-list').appendChild(imageElement);
  };

  /**
   * Appends received images to the list.
   * @param {List} images A list of urls to user images.
   */
  UserImageScreen.addUserImages = function(images) {
    for (var i = 0; i < images.length; i++) {
      var imageUrl = images[i];
      UserImageScreen.addUserImage(
          imageUrl,
          UserImageScreen.handleImageClick);
    }

    var userImageScreen = $('user-image');
    var userImageList = $('user-image-list');
    userImageList.addEventListener('keydown', function(e) {
      var prevIndex = userImageScreen.selectedUserImage_;
      var len = userImageList.children.length;
      var nextIndex;
      if (e.keyCode == 39 || e.keyCode == 40)  // right or down
        nextIndex = (prevIndex + 1) % len;
      else if (e.keyCode == 37 || e.keyCode == 38)  // left or up
        nextIndex = (prevIndex - 1 + len) % len;
      else if (e.keyCode == 13 && prevIndex == 0)
        // Enter pressed while "Take photo" button active.
        chrome.send('takePhoto');
      if (nextIndex == 0)
        // "Take photo" button: don't send a selection event to Chrome,
        // just focus element and update selected index.
        UserImageScreen.selectUserImage(0);
      else if (nextIndex)
        chrome.send('selectImage', [userImageList.children[nextIndex].src]);
      e.stopPropagation();
    });
  };

  /**
   * Selects the specified user image and shows it in preview.
   * @param {number} index The index of the image to select.
   */
  UserImageScreen.selectUserImage = function(index) {
    var userImageList = $('user-image-list');
    var userImageScreen = $('user-image');
    var prevIndex = userImageScreen.selectedUserImage_;
    if (prevIndex != -1) {
      userImageList.children[prevIndex].classList.remove('user-image-selected');
      userImageList.children[prevIndex].setAttribute('tabIndex', '-1');
    }
    if (index != -1) {
      var selectedImage = userImageList.children[index];
      selectedImage.classList.add('user-image-selected');
      selectedImage.setAttribute('tabIndex', '0');
      selectedImage.focus();
      $('user-image-preview').src = selectedImage.src;
    }
    userImageScreen.selectedUserImage_ = index;
  };

  return {
    UserImageScreen: UserImageScreen
  };
});
