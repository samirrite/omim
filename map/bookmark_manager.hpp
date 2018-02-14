#pragma once

#include "map/bookmark.hpp"
#include "map/cloud.hpp"
#include "map/user_mark_layer.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"

#include "geometry/any_rect2d.hpp"
#include "geometry/screenbase.hpp"

#include "base/macros.hpp"
#include "base/strings_bundle.hpp"

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>

class BookmarkManager final
{
  using UserMarkLayers = std::vector<std::unique_ptr<UserMarkLayer>>;
  using CategoriesCollection = std::map<df::MarkGroupID, std::unique_ptr<BookmarkCategory>>;

  using MarksCollection = std::map<df::MarkID, std::unique_ptr<UserMark>>;
  using BookmarksCollection = std::map<df::MarkID, std::unique_ptr<Bookmark>>;
  using TracksCollection = std::map<df::LineID, std::unique_ptr<Track>>;

public:
  using KMLDataCollection = std::vector<std::unique_ptr<KMLData>>;

  using AsyncLoadingStartedCallback = std::function<void()>;
  using AsyncLoadingFinishedCallback = std::function<void()>;
  using AsyncLoadingFileCallback = std::function<void(std::string const &, bool)>;

  struct AsyncLoadingCallbacks
  {
    AsyncLoadingStartedCallback m_onStarted;
    AsyncLoadingFinishedCallback m_onFinished;
    AsyncLoadingFileCallback m_onFileError;
    AsyncLoadingFileCallback m_onFileSuccess;
  };

  struct Callbacks
  {
    using GetStringsBundleFn = std::function<StringsBundle const &()>;
    using CreatedBookmarksCallback = std::function<void(std::vector<std::pair<df::MarkID, BookmarkData>> const &)>;
    using UpdatedBookmarksCallback = std::function<void(std::vector<std::pair<df::MarkID, BookmarkData>> const &)>;
    using DeletedBookmarksCallback = std::function<void(std::vector<df::MarkID> const &)>;

    template <typename StringsBundleGetter, typename CreateListener, typename UpdateListener, typename DeleteListener>
    Callbacks(StringsBundleGetter && stringsBundleGetter, CreateListener && createListener,
              UpdateListener && updateListener, DeleteListener && deleteListener)
        : m_getStringsBundle(std::forward<StringsBundleGetter>(stringsBundleGetter))
        , m_createdBookmarksCallback(std::forward<CreateListener>(createListener))
        , m_updatedBookmarksCallback(std::forward<UpdateListener>(updateListener))
        , m_deletedBookmarksCallback(std::forward<DeleteListener>(deleteListener))
    {}

    GetStringsBundleFn m_getStringsBundle;
    CreatedBookmarksCallback m_createdBookmarksCallback;
    UpdatedBookmarksCallback m_updatedBookmarksCallback;
    DeletedBookmarksCallback m_deletedBookmarksCallback;
  };

  class EditSession
  {
  public:
    EditSession(BookmarkManager & bmManager);
    ~EditSession();

    template <typename UserMarkT>
    UserMarkT * CreateUserMark(m2::PointD const & ptOrg)
    {
      return m_bmManager.CreateUserMark<UserMarkT>(ptOrg);
    }

    Bookmark * CreateBookmark(m2::PointD const & ptOrg, BookmarkData const & bm);
    Bookmark * CreateBookmark(m2::PointD const & ptOrg, BookmarkData & bm, df::MarkGroupID groupId);
    Track * CreateTrack(m2::PolylineD const & polyline, Track::Params const & p);

    template <typename UserMarkT>
    UserMarkT * GetMarkForEdit(df::MarkID markId)
    {
      return m_bmManager.GetMarkForEdit<UserMarkT>(markId);
    }

    Bookmark * GetBookmarkForEdit(df::MarkID markId);

    template <typename UserMarkT, typename F>
    void DeleteUserMarks(UserMark::Type type, F && deletePredicate)
    {
      return m_bmManager.DeleteUserMarks<UserMarkT>(type, std::move(deletePredicate));
    };

    void DeleteUserMark(df::MarkID markId);
    void DeleteBookmark(df::MarkID bmId);
    void DeleteTrack(df::LineID trackId);

    void ClearGroup(df::MarkGroupID groupId);

    void SetIsVisible(df::MarkGroupID groupId, bool visible);

    void MoveBookmark(df::MarkID bmID, df::MarkGroupID curGroupID, df::MarkGroupID newGroupID);
    void UpdateBookmark(df::MarkID bmId, BookmarkData const & bm);

    void AttachBookmark(df::MarkID bmId, df::MarkGroupID groupId);
    void DetachBookmark(df::MarkID bmId, df::MarkGroupID groupId);

    void AttachTrack(df::LineID trackId, df::MarkGroupID groupId);
    void DetachTrack(df::LineID trackId, df::MarkGroupID groupId);

    void SetCategoryName(df::MarkGroupID categoryId, std::string const & name);
    bool DeleteBmCategory(df::MarkGroupID groupId);

    void NotifyChanges();

  private:
    BookmarkManager & m_bmManager;
  };

  explicit BookmarkManager(Callbacks && callbacks);
  ~BookmarkManager();

  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);

  void SetAsyncLoadingCallbacks(AsyncLoadingCallbacks && callbacks);
  bool IsAsyncLoadingInProgress() const { return m_asyncLoadingInProgress; }

  EditSession GetEditSession();

  void UpdateViewport(ScreenBase const & screen);
  void Teardown();

  static bool IsBookmarkCategory(df::MarkGroupID groupId) { return groupId >= UserMark::BOOKMARK; }
  static bool IsBookmark(df::MarkID markId) { return UserMark::GetMarkType(markId) == UserMark::BOOKMARK; }

  template <typename UserMarkT>
  UserMarkT const * GetMark(df::MarkID markId) const
  {
    auto * mark = GetUserMark(markId);
    ASSERT(dynamic_cast<UserMarkT const *>(mark) != nullptr, ());
    return static_cast<UserMarkT const *>(mark);
  }

  UserMark const * GetUserMark(df::MarkID markId) const;
  Bookmark const * GetBookmark(df::MarkID markId) const;
  Track const * GetTrack(df::LineID trackId) const;

  df::MarkIDSet const & GetUserMarkIds(df::MarkGroupID groupId) const;
  df::LineIDSet const & GetTrackIds(df::MarkGroupID groupId) const;

  bool IsVisible(df::MarkGroupID groupId) const;

  df::MarkGroupID CreateBookmarkCategory(std::string const & name);

  std::string const & GetCategoryName(df::MarkGroupID categoryId) const;
  std::string const & GetCategoryFileName(df::MarkGroupID categoryId) const;

  df::GroupIDCollection const & GetBmGroupsIdList() const { return m_bmGroupsIdList; }
  bool HasBmCategory(df::MarkGroupID groupId) const;
  df::MarkGroupID LastEditedBMCategory();
  std::string LastEditedBMType() const;

  using TTouchRectHolder = function<m2::AnyRectD(UserMark::Type)>;
  UserMark const * FindNearestUserMark(TTouchRectHolder const & holder) const;
  UserMark const * FindNearestUserMark(m2::AnyRectD const & rect) const;
  UserMark const * FindMarkInRect(df::MarkGroupID groupId, m2::AnyRectD const & rect, double & d) const;

  /// Scans and loads all kml files with bookmarks in WritableDir.
  void LoadBookmarks();
  void LoadBookmark(std::string const & filePath, bool isTemporaryFile);

  /// Uses the same file name from which was loaded, or
  /// creates unique file name on first save and uses it every time.
  bool SaveToKMLFile(df::MarkGroupID groupId);

  StaticMarkPoint & SelectionMark() { return *m_selectionMark; }
  StaticMarkPoint const & SelectionMark() const { return *m_selectionMark; }

  MyPositionMarkPoint & MyPositionMark() { return *m_myPositionMark; }
  MyPositionMarkPoint const & MyPositionMark() const { return *m_myPositionMark; }

  void SetCloudEnabled(bool enabled);
  bool IsCloudEnabled() const;
  uint64_t GetLastSynchronizationTimestamp() const;
  std::unique_ptr<User::Subscriber> GetUserSubscriber();
  void SetInvalidTokenHandler(Cloud::InvalidTokenHandler && onInvalidToken);

  /// These functions is public for unit tests only. You shouldn't call them from client code.
  void SaveToKML(df::MarkGroupID groupId, std::ostream & s);
  void CreateCategories(KMLDataCollection && dataCollection);
  static std::string RemoveInvalidSymbols(std::string const & name);
  static std::string GenerateUniqueFileName(const std::string & path, std::string name);
  static std::string GenerateValidAndUniqueFilePathForKML(std::string const & fileName);

private:
  class MarksChangesTracker : public df::UserMarksProvider
  {
  public:
    MarksChangesTracker(BookmarkManager & bmManager) : m_bmManager(bmManager) {}

    void OnAddMark(df::MarkID markId);
    void OnDeleteMark(df::MarkID markId);
    void OnUpdateMark(df::MarkID markId);

    bool CheckChanges();
    void ResetChanges();

    // UserMarksProvider
    df::GroupIDSet GetAllGroupIds() const override;
    df::GroupIDSet const & GetDirtyGroupIds() const override { return m_dirtyGroups; }
    df::MarkIDSet const & GetCreatedMarkIds() const override { return m_createdMarks; }
    df::MarkIDSet const & GetRemovedMarkIds() const override { return m_removedMarks; }
    df::MarkIDSet const & GetUpdatedMarkIds() const override { return m_updatedMarks; }
    bool IsGroupVisible(df::MarkGroupID groupId) const override;
    bool IsGroupVisibilityChanged(df::MarkGroupID groupId) const override;
    df::MarkIDSet const & GetGroupPointIds(df::MarkGroupID groupId) const override;
    df::LineIDSet const & GetGroupLineIds(df::MarkGroupID groupId) const override;
    df::UserPointMark const * GetUserPointMark(df::MarkID markId) const override;
    df::UserLineMark const * GetUserLineMark(df::LineID lineId) const override;

  private:
    BookmarkManager & m_bmManager;

    df::MarkIDSet m_createdMarks;
    df::MarkIDSet m_removedMarks;
    df::MarkIDSet m_updatedMarks;
    df::GroupIDSet m_dirtyGroups;
  };

  template <typename UserMarkT>
  UserMarkT * CreateUserMark(m2::PointD const & ptOrg)
  {
    auto mark = std::make_unique<UserMarkT>(ptOrg);
    auto * m = mark.get();
    auto const markId = m->GetId();
    auto const groupId = static_cast<df::MarkGroupID>(m->GetMarkType());
    ASSERT(m_userMarks.count(markId) == 0, ());
    ASSERT_LESS(groupId, m_userMarkLayers.size(), ());
    m_userMarks.emplace(markId, std::move(mark));
    m_changesTracker.OnAddMark(markId);
    m_userMarkLayers[groupId]->AttachUserMark(markId);
    return m;
  }

  template <typename UserMarkT>
  UserMarkT * GetMarkForEdit(df::MarkID markId)
  {
    auto * mark = GetUserMarkForEdit(markId);
    ASSERT(dynamic_cast<UserMarkT *>(mark) != nullptr, ());
    return static_cast<UserMarkT *>(mark);
  }

  template <typename UserMarkT, typename F>
  void DeleteUserMarks(UserMark::Type type, F && deletePredicate)
  {
    std::list<df::MarkID> marksToDelete;
    for (auto markId : GetUserMarkIds(type))
    {
      if (deletePredicate(GetMark<UserMarkT>(markId)))
        marksToDelete.push_back(markId);
    }
    // Delete after iterating to avoid iterators invalidation issues.
    for (auto markId : marksToDelete)
      DeleteUserMark(markId);
  };

  UserMark * GetUserMarkForEdit(df::MarkID markId);
  void DeleteUserMark(df::MarkID markId);

  Bookmark * CreateBookmark(m2::PointD const & ptOrg, BookmarkData const & bm);
  Bookmark * CreateBookmark(m2::PointD const & ptOrg, BookmarkData & bm, df::MarkGroupID groupId);

  Bookmark * GetBookmarkForEdit(df::MarkID markId);
  void AttachBookmark(df::MarkID bmId, df::MarkGroupID groupId);
  void DetachBookmark(df::MarkID bmId, df::MarkGroupID groupId);
  void DeleteBookmark(df::MarkID bmId);

  Track * CreateTrack(m2::PolylineD const & polyline, Track::Params const & p);

  void AttachTrack(df::LineID trackId, df::MarkGroupID groupId);
  void DetachTrack(df::LineID trackId, df::MarkGroupID groupId);
  void DeleteTrack(df::LineID trackId);

  void ClearGroup(df::MarkGroupID groupId);
  void SetIsVisible(df::MarkGroupID groupId, bool visible);

  void SetCategoryName(df::MarkGroupID categoryId, std::string const & name);
  bool DeleteBmCategory(df::MarkGroupID groupId);
  void ClearCategories();

  void MoveBookmark(df::MarkID bmID, df::MarkGroupID curGroupID, df::MarkGroupID newGroupID);
  void UpdateBookmark(df::MarkID bmId, BookmarkData const & bm);

  UserMark const * GetMark(df::MarkID markId) const;

  UserMarkLayer const * GetGroup(df::MarkGroupID groupId) const;
  UserMarkLayer * GetGroup(df::MarkGroupID groupId);
  BookmarkCategory * GetBmCategory(df::MarkGroupID categoryId) const;

  Bookmark * AddBookmark(std::unique_ptr<Bookmark> && bookmark);
  Track * AddTrack(std::unique_ptr<Track> && track);

  void OnEditSessionOpened();
  void OnEditSessionClosed();
  void NotifyChanges();

  void SaveState() const;
  void LoadState();
  void NotifyAboutStartAsyncLoading();
  void NotifyAboutFinishAsyncLoading(std::shared_ptr<KMLDataCollection> && collection);
  boost::optional<std::string> GetKMLPath(std::string const & filePath);
  void NotifyAboutFile(bool success, std::string const & filePath, bool isTemporaryFile);
  void LoadBookmarkRoutine(std::string const & filePath, bool isTemporaryFile);

  void CollectDirtyGroups(df::GroupIDSet & dirtyGroups);

  void SendBookmarksChanges();
  void GetBookmarksData(df::MarkIDSet const & markIds,
                        std::vector<std::pair<df::MarkID, BookmarkData>> & data) const;
  void CheckAndCreateDefaultCategory();

  void SaveToKML(BookmarkCategory * group, std::ostream & s);

  Callbacks m_callbacks;
  MarksChangesTracker m_changesTracker;
  df::DrapeEngineSafePtr m_drapeEngine;
  AsyncLoadingCallbacks m_asyncLoadingCallbacks;
  std::atomic<bool> m_needTeardown;
  df::MarkGroupID m_nextGroupID;
  uint32_t m_openedEditSessionsCount = 0;
  bool m_loadBookmarksFinished = false;
  bool m_firstDrapeNotification = false;

  ScreenBase m_viewport;

  CategoriesCollection m_categories;
  df::GroupIDCollection m_bmGroupsIdList;

  std::string m_lastCategoryUrl;
  std::string m_lastType;
  UserMarkLayers m_userMarkLayers;

  MarksCollection m_userMarks;
  BookmarksCollection m_bookmarks;
  TracksCollection m_tracks;

  StaticMarkPoint * m_selectionMark = nullptr;
  MyPositionMarkPoint * m_myPositionMark = nullptr;

  bool m_asyncLoadingInProgress = false;
  struct BookmarkLoaderInfo
  {
    std::string m_filename;
    bool m_isTemporaryFile = false;
    BookmarkLoaderInfo() {}
    BookmarkLoaderInfo(std::string const & filename, bool isTemporaryFile)
      : m_filename(filename), m_isTemporaryFile(isTemporaryFile)
    {}
  };
  std::list<BookmarkLoaderInfo> m_bookmarkLoadingQueue;

  Cloud m_bookmarkCloud;

  DISALLOW_COPY_AND_MOVE(BookmarkManager);
};
