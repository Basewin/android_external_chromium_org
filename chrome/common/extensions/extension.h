// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_H_

#include <algorithm>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/permission_message.h"
#include "chrome/common/extensions/user_script.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/gurl.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/size.h"

class ExtensionAction;
class SkBitmap;
class Version;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace gfx {
class ImageSkia;
}

namespace extensions {
class APIPermissionSet;
class PermissionSet;

// Represents a Chrome extension.
class Extension : public base::RefCountedThreadSafe<Extension> {
 public:
  struct ManifestData;

  typedef std::vector<std::string> ScriptingWhitelist;
  typedef std::map<const std::string, linked_ptr<ManifestData> >
      ManifestDataMap;

  enum State {
    DISABLED = 0,
    ENABLED,
    // An external extension that the user uninstalled. We should not reinstall
    // such extensions on startup.
    EXTERNAL_EXTENSION_UNINSTALLED,
    // Special state for component extensions, since they are always loaded by
    // the component loader, and should never be auto-installed on startup.
    ENABLED_COMPONENT,
    NUM_STATES
  };

  // Used to record the reason an extension was disabled.
  enum DeprecatedDisableReason {
    DEPRECATED_DISABLE_UNKNOWN,
    DEPRECATED_DISABLE_USER_ACTION,
    DEPRECATED_DISABLE_PERMISSIONS_INCREASE,
    DEPRECATED_DISABLE_RELOAD,
    DEPRECATED_DISABLE_LAST,  // Not used.
  };

  enum DisableReason {
    DISABLE_NONE = 0,
    DISABLE_USER_ACTION = 1 << 0,
    DISABLE_PERMISSIONS_INCREASE = 1 << 1,
    DISABLE_RELOAD = 1 << 2,
    DISABLE_UNSUPPORTED_REQUIREMENT = 1 << 3,
    DISABLE_SIDELOAD_WIPEOUT = 1 << 4,
    DISABLE_UNKNOWN_FROM_SYNC = 1 << 5,
  };

  enum InstallType {
    INSTALL_ERROR,
    DOWNGRADE,
    REINSTALL,
    UPGRADE,
    NEW_INSTALL
  };

  enum SyncType {
    SYNC_TYPE_NONE = 0,
    SYNC_TYPE_EXTENSION,
    SYNC_TYPE_APP
  };

  // An NaCl module included in the extension.
  struct NaClModuleInfo {
    GURL url;
    std::string mime_type;
  };

  // A base class for parsed manifest data that APIs want to store on
  // the extension. Related to base::SupportsUserData, but with an immutable
  // thread-safe interface to match Extension.
  struct ManifestData {
    virtual ~ManifestData() {}
  };

  enum InitFromValueFlags {
    NO_FLAGS = 0,

    // Usually, the id of an extension is generated by the "key" property of
    // its manifest, but if |REQUIRE_KEY| is not set, a temporary ID will be
    // generated based on the path.
    REQUIRE_KEY = 1 << 0,

    // Requires the extension to have an up-to-date manifest version.
    // Typically, we'll support multiple manifest versions during a version
    // transition. This flag signals that we want to require the most modern
    // manifest version that Chrome understands.
    REQUIRE_MODERN_MANIFEST_VERSION = 1 << 1,

    // |ALLOW_FILE_ACCESS| indicates that the user is allowing this extension
    // to have file access. If it's not present, then permissions and content
    // scripts that match file:/// URLs will be filtered out.
    ALLOW_FILE_ACCESS = 1 << 2,

    // |FROM_WEBSTORE| indicates that the extension was installed from the
    // Chrome Web Store.
    FROM_WEBSTORE = 1 << 3,

    // |FROM_BOOKMARK| indicates the extension was created using a mock App
    // created from a bookmark.
    FROM_BOOKMARK = 1 << 4,

    // |FOLLOW_SYMLINKS_ANYWHERE| means that resources can be symlinks to
    // anywhere in the filesystem, rather than being restricted to the
    // extension directory.
    FOLLOW_SYMLINKS_ANYWHERE = 1 << 5,

    // |ERROR_ON_PRIVATE_KEY| means that private keys inside an
    // extension should be errors rather than warnings.
    ERROR_ON_PRIVATE_KEY = 1 << 6,

    // |WAS_INSTALLED_BY_DEFAULT| installed by default when the profile was
    // created.
    WAS_INSTALLED_BY_DEFAULT = 1 << 7,
  };

  static scoped_refptr<Extension> Create(const base::FilePath& path,
                                         Manifest::Location location,
                                         const base::DictionaryValue& value,
                                         int flags,
                                         std::string* error);

  // In a few special circumstances, we want to create an Extension and give it
  // an explicit id. Most consumers should just use the other Create() method.
  static scoped_refptr<Extension> Create(const base::FilePath& path,
      Manifest::Location location,
      const base::DictionaryValue& value,
      int flags,
      const std::string& explicit_id,
      std::string* error);

  // Valid schemes for web extent URLPatterns.
  static const int kValidWebExtentSchemes;

  // Valid schemes for host permission URLPatterns.
  static const int kValidHostPermissionSchemes;

#if defined(OS_WIN)
  static const char kExtensionRegistryPath[];
#endif

  // The mimetype used for extensions.
  static const char kMimeType[];

  // Checks to see if the extension has a valid ID.
  static bool IdIsValid(const std::string& id);

  // Returns true if the specified file is an extension.
  static bool IsExtension(const base::FilePath& file_name);

  // Fills the |info| dictionary with basic information about the extension.
  // |enabled| is injected for easier testing.
  void GetBasicInfo(bool enabled, base::DictionaryValue* info) const;

  // See Type definition in Manifest.
  Manifest::Type GetType() const;

  // Returns an absolute url to a resource inside of an extension. The
  // |extension_url| argument should be the url() from an Extension object. The
  // |relative_path| can be untrusted user input. The returned URL will either
  // be invalid() or a child of |extension_url|.
  // NOTE: Static so that it can be used from multiple threads.
  static GURL GetResourceURL(const GURL& extension_url,
                             const std::string& relative_path);
  GURL GetResourceURL(const std::string& relative_path) const {
    return GetResourceURL(url(), relative_path);
  }

  // Returns true if the resource matches a pattern in the pattern_set.
  bool ResourceMatches(const URLPatternSet& pattern_set,
                       const std::string& resource) const;

  // Returns an extension resource object. |relative_path| should be UTF8
  // encoded.
  ExtensionResource GetResource(const std::string& relative_path) const;

  // As above, but with |relative_path| following the file system's encoding.
  ExtensionResource GetResource(const base::FilePath& relative_path) const;

  // |input| is expected to be the text of an rsa public or private key. It
  // tolerates the presence or absence of bracking header/footer like this:
  //     -----(BEGIN|END) [RSA PUBLIC/PRIVATE] KEY-----
  // and may contain newlines.
  static bool ParsePEMKeyBytes(const std::string& input, std::string* output);

  // Does a simple base64 encoding of |input| into |output|.
  static bool ProducePEM(const std::string& input, std::string* output);

  // Expects base64 encoded |input| and formats into |output| including
  // the appropriate header & footer.
  static bool FormatPEMForFileOutput(const std::string& input,
                                     std::string* output,
                                     bool is_public);

  // Returns the base extension url for a given |extension_id|.
  static GURL GetBaseURLFromExtensionId(const std::string& extension_id);

  // Adds an extension to the scripting whitelist. Used for testing only.
  static void SetScriptingWhitelist(const ScriptingWhitelist& whitelist);
  static const ScriptingWhitelist* GetScriptingWhitelist();

  // Parses the host and api permissions from the specified permission |key|
  // from |manifest_|.
  bool ParsePermissions(const char* key,
                        string16* error,
                        APIPermissionSet* api_permissions,
                        URLPatternSet* host_permissions);

  // Returns true if this extension has the given permission. Prefer
  // IsExtensionWithPermissionOrSuggestInConsole when developers may be using an
  // api that requires a permission they didn't know about, e.g. open web apis.
  bool HasAPIPermission(APIPermission::ID permission) const;
  bool HasAPIPermission(const std::string& function_name) const;
  bool HasAPIPermissionForTab(int tab_id, APIPermission::ID permission) const;

  bool CheckAPIPermissionWithParam(APIPermission::ID permission,
      const APIPermission::CheckParam* param) const;

  const URLPatternSet& GetEffectiveHostPermissions() const;

  // Returns true if the extension can silently increase its permission level.
  // Users must approve permissions for unpacked and packed extensions in the
  // following situations:
  //  - when installing or upgrading packed extensions
  //  - when installing unpacked extensions that have NPAPI plugins
  //  - when either type of extension requests optional permissions
  bool CanSilentlyIncreasePermissions() const;

  // Whether the extension has access to the given URL.
  bool HasHostPermission(const GURL& url) const;

  // Whether the extension has effective access to all hosts. This is true if
  // there is a content script that matches all hosts, if there is a host
  // permission grants access to all hosts (like <all_urls>) or an api
  // permission that effectively grants access to all hosts (e.g. proxy,
  // network, etc.)
  bool HasEffectiveAccessToAllHosts() const;

  // Whether the extension effectively has all permissions (for example, by
  // having an NPAPI plugin).
  bool HasFullPermissions() const;

  // Returns the full list of permission messages that this extension
  // should display at install time.
  PermissionMessages GetPermissionMessages() const;

  // Returns the full list of permission messages that this extension
  // should display at install time. The messages are returned as strings
  // for convenience.
  std::vector<string16> GetPermissionMessageStrings() const;

  // Returns true if the extension does not require permission warnings
  // to be displayed at install time.
  bool ShouldSkipPermissionWarnings() const;

  // Sets the active |permissions|.
  void SetActivePermissions(const PermissionSet* permissions) const;

  // Gets the extension's active permission set.
  scoped_refptr<const PermissionSet> GetActivePermissions() const;

  // Whether context menu should be shown for page and browser actions.
  bool ShowConfigureContextMenus() const;

  // Returns a list of paths (relative to the extension dir) for images that
  // the browser might load (like themes and page action icons).
  std::set<base::FilePath> GetBrowserImages() const;

  // Gets the fully resolved absolute launch URL.
  GURL GetFullLaunchURL() const;

  // Returns true if this extension can execute script on a page. If a
  // UserScript object is passed, permission to run that specific script is
  // checked (using its matches list). Otherwise, permission to execute script
  // programmatically is checked (using the extension's host permission).
  //
  // This method is also aware of certain special pages that extensions are
  // usually not allowed to run script on.
  bool CanExecuteScriptOnPage(const GURL& document_url,
                              const GURL& top_document_url,
                              int tab_id,
                              const UserScript* script,
                              std::string* error) const;

  // Returns true if this extension is a COMPONENT extension, or if it is
  // on the whitelist of extensions that can script all pages.
  bool CanExecuteScriptEverywhere() const;

  // Returns true if this extension is allowed to obtain the contents of a
  // page as an image.  Since a page may contain sensitive information, this
  // is restricted to the extension's host permissions as well as the
  // extension page itself.
  bool CanCaptureVisiblePage(const GURL& page_url,
                             int tab_id,
                             std::string* error) const;

  // Returns true if this extension updates itself using the extension
  // gallery.
  bool UpdatesFromGallery() const;

  // Returns true if this extension or app includes areas within |origin|.
  bool OverlapsWithOrigin(const GURL& origin) const;

  // Returns the sync bucket to use for this extension.
  SyncType GetSyncType() const;

  // Returns true if the extension should be synced.
  bool IsSyncable() const;

  // Returns true if the extension requires a valid ordinal for sorting, e.g.,
  // for displaying in a launcher or new tab page.
  bool RequiresSortOrdinal() const;

  // Returns true if the extension should be displayed in the app launcher.
  bool ShouldDisplayInAppLauncher() const;

  // Returns true if the extension should be displayed in the browser NTP.
  bool ShouldDisplayInNewTabPage() const;

  // Returns true if the extension should be displayed in the extension
  // settings page (i.e. chrome://extensions).
  bool ShouldDisplayInExtensionSettings() const;

  // Gets the tab-specific host permissions of |tab_id|, or NULL if there
  // aren't any.
  scoped_refptr<const PermissionSet> GetTabSpecificPermissions(int tab_id)
      const;

  // Updates the tab-specific permissions of |tab_id| to include those from
  // |permissions|.
  void UpdateTabSpecificPermissions(
      int tab_id,
      scoped_refptr<const PermissionSet> permissions) const;

  // Clears the tab-specific permissions of |tab_id|.
  void ClearTabSpecificPermissions(int tab_id) const;

  // Get the manifest data associated with the key, or NULL if there is none.
  // Can only be called after InitValue is finished.
  ManifestData* GetManifestData(const std::string& key) const;

  // Sets |data| to be associated with the key. Takes ownership of |data|.
  // Can only be called before InitValue is finished. Not thread-safe;
  // all SetManifestData calls should be on only one thread.
  void SetManifestData(const std::string& key, ManifestData* data);

  // Accessors:

  const base::FilePath& path() const { return path_; }
  const GURL& url() const { return extension_url_; }
  Manifest::Location location() const;
  const std::string& id() const;
  const Version* version() const { return version_.get(); }
  const std::string VersionString() const;
  const std::string& name() const { return name_; }
  const std::string& non_localized_name() const { return non_localized_name_; }
  // Base64-encoded version of the key used to sign this extension.
  // In pseudocode, returns
  // base::Base64Encode(RSAPrivateKey(pem_file).ExportPublicKey()).
  const std::string& public_key() const { return public_key_; }
  const std::string& description() const { return description_; }
  int manifest_version() const { return manifest_version_; }
  bool converted_from_user_script() const {
    return converted_from_user_script_;
  }
  const std::vector<NaClModuleInfo>& nacl_modules() const {
    return nacl_modules_;
  }
  const PermissionSet* optional_permission_set() const {
    return optional_permission_set_.get();
  }
  const PermissionSet* required_permission_set() const {
    return required_permission_set_.get();
  }
  // Returns the temporary APIPermissionSet used in initialization.
  // (NULL after initialization is completed.)
  APIPermissionSet* initial_api_permissions() {
    return initial_api_permissions_.get();
  }
  const APIPermissionSet* initial_api_permissions() const {
    return initial_api_permissions_.get();
  }
  // Appends |new_warning[s]| to install_warnings_.
  void AddInstallWarning(const InstallWarning& new_warning);
  void AddInstallWarnings(const std::vector<InstallWarning>& new_warnings);
  const std::vector<InstallWarning>& install_warnings() const {
    return install_warnings_;
  }
  const extensions::Manifest* manifest() const {
    return manifest_.get();
  }
  bool wants_file_access() const { return wants_file_access_; }
  // TODO(rdevlin.cronin): This is needed for ContentScriptsHandler, and should
  // be moved out as part of crbug.com/159265. This should not be used anywhere
  // else.
  void set_wants_file_access(bool wants_file_access) {
    wants_file_access_ = wants_file_access;
  }
  int creation_flags() const { return creation_flags_; }
  bool from_webstore() const { return (creation_flags_ & FROM_WEBSTORE) != 0; }
  bool from_bookmark() const { return (creation_flags_ & FROM_BOOKMARK) != 0; }
  bool was_installed_by_default() const {
    return (creation_flags_ & WAS_INSTALLED_BY_DEFAULT) != 0;
  }

  // App-related.
  bool is_app() const;
  bool is_platform_app() const;
  bool is_hosted_app() const;
  bool is_legacy_packaged_app() const;
  bool is_extension() const;
  bool is_storage_isolated() const { return is_storage_isolated_; }
  bool can_be_incognito_enabled() const;
  void AddWebExtentPattern(const URLPattern& pattern);
  const URLPatternSet& web_extent() const { return extent_; }
  const std::string& launch_local_path() const { return launch_local_path_; }
  const std::string& launch_web_url() const { return launch_web_url_; }
  extension_misc::LaunchContainer launch_container() const {
    return launch_container_;
  }
  int launch_width() const { return launch_width_; }
  int launch_height() const { return launch_height_; }

  // Theme-related.
  bool is_theme() const;

 private:
  friend class base::RefCountedThreadSafe<Extension>;

  class RuntimeData {
   public:
    RuntimeData();
    explicit RuntimeData(const PermissionSet* active);
    ~RuntimeData();

    void SetActivePermissions(const PermissionSet* active);
    scoped_refptr<const PermissionSet> GetActivePermissions() const;

    scoped_refptr<const PermissionSet> GetTabSpecificPermissions(int tab_id)
        const;
    void UpdateTabSpecificPermissions(
        int tab_id,
        scoped_refptr<const PermissionSet> permissions);
    void ClearTabSpecificPermissions(int tab_id);

   private:
    friend class base::RefCountedThreadSafe<RuntimeData>;

    scoped_refptr<const PermissionSet> active_permissions_;

    typedef std::map<int, scoped_refptr<const PermissionSet> >
        TabPermissionsMap;
    TabPermissionsMap tab_specific_permissions_;
  };

  // Chooses the extension ID for an extension based on a variety of criteria.
  // The chosen ID will be set in |manifest|.
  static bool InitExtensionID(extensions::Manifest* manifest,
                              const base::FilePath& path,
                              const std::string& explicit_id,
                              int creation_flags,
                              string16* error);

  // Returns true if this extension id is from a trusted provider.
  static bool IsTrustedId(const std::string& id);

  Extension(const base::FilePath& path,
            scoped_ptr<extensions::Manifest> manifest);
  virtual ~Extension();

  // Initialize the extension from a parsed manifest.
  // TODO(aa): Rename to just Init()? There's no Value here anymore.
  // TODO(aa): It is really weird the way this class essentially contains a copy
  // of the underlying DictionaryValue in its members. We should decide to
  // either wrap the DictionaryValue and go with that only, or we should parse
  // into strong types and discard the value. But doing both is bad.
  bool InitFromValue(int flags, string16* error);

  // The following are helpers for InitFromValue to load various features of the
  // extension from the manifest.

  bool LoadAppIsolation(string16* error);

  bool LoadRequiredFeatures(string16* error);
  bool LoadName(string16* error);
  bool LoadVersion(string16* error);

  bool LoadAppFeatures(string16* error);
  bool LoadExtent(const char* key,
                  URLPatternSet* extent,
                  const char* list_error,
                  const char* value_error,
                  string16* error);
  bool LoadLaunchContainer(string16* error);
  bool LoadLaunchURL(string16* error);

  bool LoadSharedFeatures(string16* error);
  bool LoadDescription(string16* error);
  bool LoadManifestVersion(string16* error);
  bool LoadNaClModules(string16* error);
  bool LoadTextToSpeechVoices(string16* error);

  // Returns true if the extension has more than one "UI surface". For example,
  // an extension that has a browser action and a page action.
  bool HasMultipleUISurfaces() const;

  // Updates the launch URL and extents for the extension using the given
  // |override_url|.
  void OverrideLaunchUrl(const GURL& override_url);

  // Custom checks for the experimental permission that can't be expressed in
  // _permission_features.json.
  bool CanSpecifyExperimentalPermission() const;

  // Checks whether the host |pattern| is allowed for this extension, given API
  // permissions |permissions|.
  bool CanSpecifyHostPermission(const URLPattern& pattern,
                                const APIPermissionSet& permissions) const;

  bool CheckMinimumChromeVersion(string16* error) const;

  // Check that platform app features are valid. Called after InitFromValue.
  bool CheckPlatformAppFeatures(string16* error) const;

  // Check that features don't conflict. Called after InitFromValue.
  bool CheckConflictingFeatures(string16* error) const;

  // The extension's human-readable name. Name is used for display purpose. It
  // might be wrapped with unicode bidi control characters so that it is
  // displayed correctly in RTL context.
  // NOTE: Name is UTF-8 and may contain non-ascii characters.
  std::string name_;

  // A non-localized version of the extension's name. This is useful for
  // debug output.
  std::string non_localized_name_;

  // The version of this extension's manifest. We increase the manifest
  // version when making breaking changes to the extension system.
  // Version 1 was the first manifest version (implied by a lack of a
  // manifest_version attribute in the extension's manifest). We initialize
  // this member variable to 0 to distinguish the "uninitialized" case from
  // the case when we know the manifest version actually is 1.
  int manifest_version_;

  // The absolute path to the directory the extension is stored in.
  base::FilePath path_;

  // Defines the set of URLs in the extension's web content.
  URLPatternSet extent_;

  // The extension runtime data.
  mutable base::Lock runtime_data_lock_;
  mutable RuntimeData runtime_data_;

  // The API permission set; used during extension initialization.
  // Cleared after permissions are finalized by SetActivePermissions.
  scoped_ptr<APIPermissionSet> initial_api_permissions_;

  // The set of permissions the extension can request at runtime.
  scoped_refptr<const PermissionSet> optional_permission_set_;

  // The extension's required / default set of permissions.
  scoped_refptr<const PermissionSet> required_permission_set_;

  // Any warnings that occurred when trying to create/parse the extension.
  std::vector<InstallWarning> install_warnings_;

  // The base extension url for the extension.
  GURL extension_url_;

  // The extension's version.
  scoped_ptr<Version> version_;

  // An optional longer description of the extension.
  std::string description_;

  // True if the extension was generated from a user script. (We show slightly
  // different UI if so).
  bool converted_from_user_script_;

  // Optional list of NaCl modules and associated properties.
  std::vector<NaClModuleInfo> nacl_modules_;

  // The public key used to sign the contents of the crx package.
  std::string public_key_;

  // The manifest from which this extension was created.
  scoped_ptr<Manifest> manifest_;

  // Stored parsed manifest data.
  ManifestDataMap manifest_data_;

  // Set to true at the end of InitValue when initialization is finished.
  bool finished_parsing_manifest_;

  // Ensures that any call to GetManifestData() prior to finishing
  // initialization happens from the same thread (this can happen when certain
  // parts of the initialization process need information from previous parts).
  base::ThreadChecker thread_checker_;

  // Whether this extension requests isolated storage.
  bool is_storage_isolated_;

  // The local path inside the extension to use with the launcher.
  std::string launch_local_path_;

  // A web url to use with the launcher. Note that this might be relative or
  // absolute. If relative, it is relative to web_origin.
  std::string launch_web_url_;

  // The window type that an app's manifest specifies to launch into.
  // This is not always the window type an app will open into, because
  // users can override the way each app launches.  See
  // ExtensionPrefs::GetLaunchContainer(), which looks at a per-app pref
  // to decide what container an app will launch in.
  extension_misc::LaunchContainer launch_container_;

  // The default size of the container when launching. Only respected for
  // containers like panels and windows.
  int launch_width_;
  int launch_height_;

  // Should this app be shown in the app launcher.
  bool display_in_launcher_;

  // Should this app be shown in the browser New Tab Page.
  bool display_in_new_tab_page_;

  // Whether the extension has host permissions or user script patterns that
  // imply access to file:/// scheme URLs (the user may not have actually
  // granted it that access).
  bool wants_file_access_;

  // The flags that were passed to InitFromValue.
  int creation_flags_;

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

typedef std::vector<scoped_refptr<const Extension> > ExtensionList;
typedef std::set<std::string> ExtensionIdSet;
typedef std::vector<std::string> ExtensionIdList;

// Handy struct to pass core extension info around.
struct ExtensionInfo {
  ExtensionInfo(const base::DictionaryValue* manifest,
                const std::string& id,
                const base::FilePath& path,
                Manifest::Location location);
  ~ExtensionInfo();

  scoped_ptr<base::DictionaryValue> extension_manifest;
  std::string extension_id;
  base::FilePath extension_path;
  Manifest::Location extension_location;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInfo);
};

struct UnloadedExtensionInfo {
  extension_misc::UnloadedExtensionReason reason;

  // Was the extension already disabled?
  bool already_disabled;

  // The extension being unloaded - this should always be non-NULL.
  const Extension* extension;

  UnloadedExtensionInfo(
      const Extension* extension,
      extension_misc::UnloadedExtensionReason reason);
};

// The details sent for EXTENSION_PERMISSIONS_UPDATED notifications.
struct UpdatedExtensionPermissionsInfo {
  enum Reason {
    ADDED,    // The permissions were added to the extension.
    REMOVED,  // The permissions were removed from the extension.
  };

  Reason reason;

  // The extension who's permissions have changed.
  const Extension* extension;

  // The permissions that have changed. For Reason::ADDED, this would contain
  // only the permissions that have added, and for Reason::REMOVED, this would
  // only contain the removed permissions.
  const PermissionSet* permissions;

  UpdatedExtensionPermissionsInfo(
      const Extension* extension,
      const PermissionSet* permissions,
      Reason reason);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_H_
