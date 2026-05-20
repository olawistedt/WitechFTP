#ifndef LANGSTRINGS_H
#define LANGSTRINGS_H

struct LangStrings {
  const char *savedSites;
  const char *connectBtn;
  const char *disconnectBtn;
  const char *connectingBtn;
  const char *stopBtn;
  const char *labelHost;
  const char *labelUsername;
  const char *labelPassword;
  const char *tooltipConnected;
  const char *tooltipDisconnected;
  const char *placeholderLocalPath;
  const char *placeholderRemotePath;
  const char *placeholderStatusLog;
  const char *colName;
  const char *colSize;
  const char *colDate;
  const char *langBtnLabel;
  // Context menus
  const char *menuRefresh;
  const char *menuCreateFolder;
  const char *menuCreateNewFolder;
  const char *menuRename;
  const char *menuUploadFolder;
  const char *menuUploadFile;
  const char *menuDelete;
  const char *menuDeleteFolder;
  const char *menuDownloadFile;
  const char *menuDownloadFolder;
  const char *menuDeleteFile;
  // Dialogs – connection
  const char *dlgConnecting;
  const char *dlgFtpPasswordTitle;
  const char *dlgFtpPasswordPrompt;
  const char *dlgConnErrTitle;
  const char *dlgNotConnTitle;
  const char *dlgNotConnUploadMsg;
  const char *dlgNotConnUploadFolderMsg;
  // Dialogs – delete (remote, specific)
  const char *dlgDeleteFileTitle;
  const char *dlgDeleteFileMsg;
  const char *dlgDeleteFolderTitle;
  const char *dlgDeleteFolderMsg;
  // Dialogs – delete (local, generic with type word)
  const char *wordFolder;
  const char *wordFile;
  const char *wordFolderDef;
  const char *wordFileDef;
  const char *dlgDeleteTitle;
  const char *dlgDeleteMsg;
  const char *dlgDeleteMultiTitle;
  const char *dlgDeleteMultiMsg;
  // Dialogs – errors
  const char *dlgErrTitle;
  const char *dlgErrDeleteMsg;
  const char *dlgErrCreateFolderMsg;
  const char *dlgErrRenameMsg;
  // Dialogs – create folder
  const char *dlgCreateFolderTitle;
  const char *dlgFolderNamePrompt;
  // Dialogs – file already exists (local rename)
  const char *dlgFileExistsTitle;
  const char *dlgFileExistsMsg;
  // Dialogs – overwrite on download
  const char *dlgOverwriteTitle;
  const char *dlgOverwriteMsg;
  // Dialogs – file not found on upload
  const char *dlgFileNotFoundTitle;
  const char *dlgFileNotFoundMsg;
  // Dialogs – quit while copying
  const char *dlgQuitCopyingTitle;
  const char *dlgQuitCopyingMsg;
  // Dialogs – transfer count mismatch
  const char *dlgTransferMismatchMsg;
  // Log
  const char *logDeleted;
};

// Index: 0=sv, 1=en, 2=es, 3=ja
constexpr int kNumLangs = 4;
extern const LangStrings *s_langs[kNumLangs];

#endif  // LANGSTRINGS_H
