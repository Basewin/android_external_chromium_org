// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_CONTEXT_H_
#define WEBKIT_FILEAPI_ISOLATED_CONTEXT_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/mount_points.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {
class FileSystemURL;
}

namespace fileapi {

// Manages isolated filesystem mount points which have no well-known names
// and are identified by a string 'filesystem ID', which usually just looks
// like random value.
// This type of filesystem can be created on the fly and may go away when it has
// no references from renderers.
// Files in an isolated filesystem are registered with corresponding names and
// identified by a filesystem URL like:
//
//   filesystem:<origin>/isolated/<filesystem_id>/<name>/relative/path
//
// Some methods of this class are virtual just for mocking.
//
class WEBKIT_STORAGE_EXPORT IsolatedContext : public MountPoints {
 public:
  class WEBKIT_STORAGE_EXPORT FileInfoSet {
   public:
    FileInfoSet();
    ~FileInfoSet();

    // Add the given |path| to the set and populates |registered_name| with
    // the registered name assigned for the path. |path| needs to be
    // absolute and should not contain parent references.
    // Return false if the |path| is not valid and could not be added.
    bool AddPath(const base::FilePath& path, std::string* registered_name);

    // Add the given |path| with the |name|.
    // Return false if the |name| is already registered in the set or
    // is not valid and could not be added.
    bool AddPathWithName(const base::FilePath& path, const std::string& name);

    const std::set<MountPointInfo>& fileset() const { return fileset_; }

   private:
    std::set<MountPointInfo> fileset_;
  };

  // The instance is lazily created per browser process.
  static IsolatedContext* GetInstance();

  // Returns true if the given filesystem type is managed by IsolatedContext
  // (i.e. if the given |type| is Isolated or External).
  // TODO(kinuko): needs a better function name.
  static bool IsIsolatedType(FileSystemType type);

  // Registers a new isolated filesystem with the given FileInfoSet |files|
  // and returns the new filesystem_id.  The files are registered with their
  // register_name as their keys so that later we can resolve the full paths
  // for the given name.  We only expose the name and the ID for the
  // newly created filesystem to the renderer for the sake of security.
  //
  // The renderer will be sending filesystem requests with a virtual path like
  // '/<filesystem_id>/<registered_name>/<relative_path_from_the_dropped_path>'
  // for which we could crack in the browser process by calling
  // CrackIsolatedPath to get the full path.
  //
  // For example: if a dropped file has a path like '/a/b/foo' and we register
  // the path with the name 'foo' in the newly created filesystem.
  // Later if the context is asked to crack a virtual path like '/<fsid>/foo'
  // it can properly return the original path '/a/b/foo' by looking up the
  // internal mapping.  Similarly if a dropped entry is a directory and its
  // path is like '/a/b/dir' a virtual path like '/<fsid>/dir/foo' can be
  // cracked into '/a/b/dir/foo'.
  //
  // Note that the path in |fileset| that contains '..' or is not an
  // absolute path is skipped and is not registered.
  std::string RegisterDraggedFileSystem(const FileInfoSet& files);

  // Registers a new isolated filesystem for a given |path| of filesystem
  // |type| filesystem and returns a new filesystem ID.
  // |path| must be an absolute path which has no parent references ('..').
  // If |register_name| is non-null and has non-empty string the path is
  // registered as the given |register_name|, otherwise it is populated
  // with the name internally assigned to the path.
  std::string RegisterFileSystemForPath(FileSystemType type,
                                        const base::FilePath& path,
                                        std::string* register_name);

  // Revokes all filesystem(s) registered for the given path.
  // This is assumed to be called when the registered path becomes
  // globally invalid, e.g. when a device for the path is detached.
  //
  // Note that this revokes the filesystem no matter how many references it has.
  // It is ok to call this for the path that has no associated filesystems.
  // Note that this only works for the filesystems registered by
  // |RegisterFileSystemForPath|.
  void RevokeFileSystemByPath(const base::FilePath& path);

  // Adds a reference to a filesystem specified by the given filesystem_id.
  void AddReference(const std::string& filesystem_id);

  // Removes a reference to a filesystem specified by the given filesystem_id.
  // If the reference count reaches 0 the isolated context gets destroyed.
  // It is OK to call this on the filesystem that has been already deleted
  // (e.g. by RevokeFileSystemByPath).
  void RemoveReference(const std::string& filesystem_id);

  // Returns a set of dragged MountPointInfos registered for the
  // |filesystem_id|.
  // The filesystem_id must be pointing to a dragged file system
  // (i.e. must be the one registered by RegisterDraggedFileSystem).
  // Returns false if the |filesystem_id| is not valid.
  bool GetDraggedFileInfo(const std::string& filesystem_id,
                          std::vector<MountPointInfo>* files) const;

  // MountPoints overrides.
  virtual bool HandlesFileSystemMountType(FileSystemType type) const OVERRIDE;
  virtual bool RevokeFileSystem(const std::string& filesystem_id) OVERRIDE;
  virtual bool GetRegisteredPath(const std::string& filesystem_id,
                                 base::FilePath* path) const OVERRIDE;
  virtual bool CrackVirtualPath(const base::FilePath& virtual_path,
                                std::string* filesystem_id,
                                FileSystemType* type,
                                base::FilePath* path) const OVERRIDE;
  virtual FileSystemURL CrackURL(const GURL& url) const OVERRIDE;
  virtual FileSystemURL CreateCrackedFileSystemURL(
      const GURL& origin,
      FileSystemType type,
      const base::FilePath& path) const OVERRIDE;

  // Returns the virtual root path that looks like /<filesystem_id>.
  base::FilePath CreateVirtualRootPath(const std::string& filesystem_id) const;

 private:
  friend struct base::DefaultLazyInstanceTraits<IsolatedContext>;

  // Represents each file system instance (defined in the .cc).
  class Instance;

  typedef std::map<std::string, Instance*> IDToInstance;

  // Reverse map from registered path to IDs.
  typedef std::map<base::FilePath, std::set<std::string> > PathToID;

  // Obtain an instance of this class via GetInstance().
  IsolatedContext();
  virtual ~IsolatedContext();

  // MountPoints overrides.
  virtual FileSystemURL CrackFileSystemURL(
      const FileSystemURL& url) const OVERRIDE;

  // Unregisters a file system of given |filesystem_id|. Must be called with
  // lock_ held.  Returns true if the file system is unregistered.
  bool UnregisterFileSystem(const std::string& filesystem_id);

  // Returns a new filesystem_id.  Called with lock.
  std::string GetNewFileSystemId() const;

  // This lock needs to be obtained when accessing the instance_map_.
  mutable base::Lock lock_;

  IDToInstance instance_map_;
  PathToID path_to_id_map_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_CONTEXT_H_
