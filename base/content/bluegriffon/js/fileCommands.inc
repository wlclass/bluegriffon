
var cmdOpen =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    window.openDialog("chrome://bluegriffon/content/dialogs/openLocation.xul","_blank",
                      "chrome,modal,titlebar", "tab");
  }
};

var cmdOpenFile =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;    // we can always do this
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    try {
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(Components.interfaces.nsIFilePicker);
      fp.init(window,
              gDialog.bundleOpenLocation.getString("chooseFileDialogTitle"),
              Components.interfaces.nsIFilePicker.modeOpen);
      
      fp.appendFilters(Components.interfaces.nsIFilePicker.filterHTML);
      fp.appendFilter(gDialog.bundleOpenLocation.getString("PHPfiles"), "*.php");
      fp.appendFilters(Components.interfaces.nsIFilePicker.filterText);
      fp.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
  
      if (fp.show() == Components.interfaces.nsIFilePicker.returnOK
          && fp.fileURL.spec
          && fp.fileURL.spec.length > 0)
      {
        StoreUrlInLocationDB(fp.fileURL.spec);
        OpenFile(fp.fileURL.spec, true);
      }
    }
    catch(ex) {
    }
  }
};

var cmdSave =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    // Always allow saving when editing a remote document,
    //  otherwise the document modified state would prevent that
    //  when you first open a remote file.
    try {
      var docUrl = EditorUtils.getDocumentUrl();
      return EditorUtils.isDocumentEditable()
              && (GetCurrentViewMode() == "source"
                  || EditorUtils.isDocumentModified()
                  || UrlUtils.isUrlOfBlankDocument(docUrl));
    } catch (e) {return false;}
  },
  
  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    var result = false;
    var editor = EditorUtils.getCurrentEditor();
    if (editor)
      try {
        var mode = GetCurrentViewMode();
        if (mode == "source") {
          var editorElement = EditorUtils.getCurrentEditorElement();
          var sourceIframe = editorElement.previousSibling;
          var sourceEditor = sourceIframe.contentWindow.wrappedJSObject.gEditor;
          var source = sourceEditor.getValue();

          result = FileUtils.saveSourceDocument(source,
                     UrlUtils.isUrlOfBlankDocument(EditorUtils.getDocumentUrl()),
                     false,
                     EditorUtils.getCurrentDocumentMimeType());
          if (result) { 
            // we must update the original source to detect if the document has
            // changed when we leave source mode; b=479
            sourceIframe.setUserData("lastSaved", source, null);
          }
        }
        else {
          EditorUtils.cleanup();
          result = FileUtils.saveDocument(
                     UrlUtils.isUrlOfBlankDocument(EditorUtils.getDocumentUrl()),
                     false,
                     EditorUtils.getCurrentDocumentMimeType());
        }
        window.content.focus();
        window.updateCommands("style");
        if (result)
          StoreUrlInLocationDB(EditorUtils.getDocumentUrl());
      }
      catch (e) {}
    return result;
  }
}

var cmdSaveAs =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return EditorUtils.isDocumentEditable();
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    var result = false;
    var editor = EditorUtils.getCurrentEditor();
    if (editor)
      try {
        var mode = GetCurrentViewMode();
        if (mode == "source") {
          var editorElement = EditorUtils.getCurrentEditorElement();
          var sourceIframe = editorElement.previousSibling;
          var sourceEditor = sourceIframe.contentWindow.wrappedJSObject.gEditor;
          var source = sourceEditor.getValue();

          result = FileUtils.saveSourceDocument(source,
                     true,
                     false,
                     EditorUtils.getCurrentDocumentMimeType());
          if (result) { 
            // we must update the original source to detect if the document has
            // changed when we leave source mode; b=479
            sourceIframe.setUserData("lastSaved", source, null);
          }
        }
        else {
          EditorUtils.cleanup();
          result = FileUtils.saveDocument(
                     true,
                     false,
                     EditorUtils.getCurrentDocumentMimeType());
        }
        window.content.focus();
        if (result)
          StoreUrlInLocationDB(EditorUtils.getDocumentUrl());
      }
      catch (e) {}
    return result;
  }
}

var cmdCloseTab =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    return true;
  },
  
  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    var editorElement = EditorUtils.getCurrentEditorElement();
    if (!editorElement) // sanity check
      window.close();

    var mode = editorElement.parentNode.getAttribute("previousMode");
    if (mode == "source") {
      if (!ToggleViewMode(gDialog.wysiwygModeButton))
        return;
    }
    if (EditorUtils.isDocumentModified()) {
      var rv = PromptUtils.confirmWithTitle(
                             L10NUtils.getString("FileNotSaved"),
                             L10NUtils.getString("SaveFileBeforeClosing"),
                             L10NUtils.getString("YesSaveFile"),
                             L10NUtils.getString("DontCloseTab"),
                             L10NUtils.getString("NoDiscardChanges"));
       switch(rv) {
         case 1:
           SaveCurrentTabLocation();
           return rv;
         case 0:
           if (!cmdSave.doCommand()) {
             SaveCurrentTabLocation();
             return 1;
           }
         default: break;
       }
    }
    SaveCurrentTabLocation();
    doCloseTab(EditorUtils.getCurrentTabEditor().selectedTab);
    return rv;
  }
}

function StoreUrlInLocationDB(url)
{
  RecentPagesHandler.saveRecentFilesPrefs();
  RecentPagesHandler.buildRecentPagesMenu();
  var file = Components.classes["@mozilla.org/file/directory_service;1"]
                       .getService(Components.interfaces.nsIProperties)
                       .get("ProfD", Components.interfaces.nsIFile);
  file.append("bgLocations.sqlite");
  
  var storageService = Components.classes["@mozilla.org/storage/service;1"]
                          .getService(Components.interfaces.mozIStorageService);
  var dbConn = storageService.openDatabase(file);
  try {
    var statement = dbConn.createStatement(
      "INSERT INTO 'bgLocations' ('query','querydate') VALUES(?1,?2)");
  
    statement.bindUTF8StringParameter(0, url);
    statement.bindInt64Parameter(1, Date.parse(new Date()));
  
    statement.execute();
    statement.finalize();

    dbConn.close();
  }
  catch (e) {} // already exists in table
}
