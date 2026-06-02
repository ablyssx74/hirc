/*
 * Copyright 2026, Kris Beazley hirc@epluribusunix.net
 * All rights reserved. Distributed under the terms of the MIT license.
 */

// Haiku Core Application & System Kit Headers
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Font.h>
#include <OS.h>
#include <SupportKit.h>

// Storage, Path Finder & System File Kits
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <Path.h>

// Vector Graphics & Interface Icons Elements
// #include <IconUtils.h>   
// #include <Bitmap.h>   

// Interface Controls & Layout Managers
#include <Button.h>
#include <TextControl.h>
#include <TextView.h>
#include <ListView.h>
#include <OutlineListView.h>
#include <ScrollView.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <Alert.h>
#include <CheckBox.h>
#include <MenuField.h>

// Data Types, Sockets & Networking Kit Headers
#include <String.h>
#include <NetworkAddress.h>
#include <SecureSocket.h>
#include <Socket.h>

// Standard C/C++ STL & POSIX Includes
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <map>
#include "nlohmann/json.hpp"


using json = nlohmann::json;

struct ServerConfig {
    std::string name;
    std::string host;
    uint16 port;
    std::string nick;
    std::string pass;
    std::vector<std::string> autojoin; 
    bool autoConnect; 
    bool hideStatusMessages = false; 
    bool autoReconnect; 
    int32 serverListFontSize = 12;
    int32 chatLogFontSize = 12;
    int32 userListFontSize = 12;    
 
};

struct Config {
    bool debugEnable = false;
    bool hideStatusMessages = false; 
    std::vector<ServerConfig> servers;
    int32 serverListFontSize = 12;
    int32 chatLogFontSize = 12;
    int32 userListFontSize = 12;
} cfg;

int selectedConfig = 0;

void ensure_config_dir() {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("hirc");
        create_directory(path.Path(), 0755);
    }
}

void save_config() {
    ensure_config_dir();
    json j;
    j["debugEnable"] = cfg.debugEnable;

    
    
    j["serverListFontSize"] = cfg.serverListFontSize;
    j["chatLogFontSize"] = cfg.chatLogFontSize;
    j["userListFontSize"] = cfg.userListFontSize;
    
    json serverArray = json::array();
    for (const auto& srv : cfg.servers) {
        json s;        
        s["name"] = srv.name;
        s["host"] = srv.host;
        s["port"] = srv.port;
        s["nick"] = srv.nick;
        s["pass"] = srv.pass;
        s["autoConnect"] = srv.autoConnect; 
        s["autoReconnect"] = srv.autoReconnect;
        s["hideStatusMessages"] = srv.hideStatusMessages; 
        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;
        serverArray.push_back(s);
    }
    j["servers"] = serverArray;

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("hirc/hircConfig.txt");
        std::ofstream outfile(path.Path());
        if (outfile.is_open()) {
            outfile << j.dump(4);
            outfile.close();
        }
    }
}

void load_config() {
    ensure_config_dir();
    cfg.debugEnable = false;
    cfg.hideStatusMessages = false;     
 
    // Empty the global array vector
    cfg.servers.clear(); 

    BPath path;
    bool mustSaveDefaults = false;

    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("hirc/hircConfig.txt");
        std::ifstream infile(path.Path());        
        if (infile.is_open()) {
            try {
                json j = json::parse(infile);
                
                cfg.debugEnable = j.value("debugEnable", false);             
                cfg.serverListFontSize = j.value("serverListFontSize", (int32)12);
    			cfg.chatLogFontSize    = j.value("chatLogFontSize", (int32)12);
    			
    			cfg.userListFontSize   = j.value("userListFontSize", (int32)12);  
                if (j.contains("servers") && j["servers"].is_array()) {
                    for (const auto& s : j["servers"]) {
                        ServerConfig srv;
                        srv.name = s.value("name", "Unknown Server");
                        srv.host = s.value("host", "127.0.0.1");
                        srv.port = s.value("port", (uint16)6697);
                        srv.nick = s.value("nick", "HaikuUser");
                        srv.pass = s.value("pass", "");
                        srv.autoReconnect = s.value("autoReconnect", false); 
                        srv.autoConnect = s.value("autoConnect", false); 
                        srv.hideStatusMessages = s.value("hideStatusMessages", false);
                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }
                        
                        bool isDuplicate = false;
                        for (const auto& existing : cfg.servers) {
                            if (existing.host == srv.host && existing.name == srv.name) {
                                isDuplicate = true;
                                break;
                            }
                        }
                        if (!isDuplicate) {
                            cfg.servers.push_back(srv);
                        }
                    }
                }
            } catch(...) {
                mustSaveDefaults = true;
            }
            infile.close();
        } else {
            mustSaveDefaults = true;
        }
    }

    if (mustSaveDefaults || cfg.servers.empty()) {
        cfg.servers.clear();        
        
        // 1. Seed the random number engine using the high-precision system clock
        srand(static_cast<unsigned int>(real_time_clock_usecs()));
        
        // 2. Generate a random 4-digit numeric suffix between 1000 and 9999
        int randomSuffix = 1000 + (rand() % 9000);
        
        // 3. Assemble the dynamic nickname string parameter
        BString dynamicNick;
        dynamicNick << "HaikuIRCUser" << randomSuffix;

        // 4. Assign the generated unique nickname back to your server fallback blocks
        ServerConfig libera = {"Libera Chat", "irc.libera.chat", 6697, dynamicNick.String(), "", {"#ubuntu", "#linux"}};
        ServerConfig oftc   = {"OFTC", "irc.oftc.net", 6697, dynamicNick.String(), "", {"#haiku"}};
        
        cfg.servers.push_back(libera);
        cfg.servers.push_back(oftc);
        save_config(); 
    }

}






// Define application messages
enum {
    MSG_START_SIRC       = 'strt',
    MSG_SEND_MESSAGE     = 'send',
    MSG_IRC_RECEIVED     = 'recv',
    MSG_CONNECT_LIBERA   = 'cnlb',
    MSG_TOGGLE_AUTOJOIN  = 'tgaj',
    MSG_TOGGLE_AUTOCONNECT = 'tgac',
    MSG_CONNECT_OFTC     = 'cnof',
    MSG_OPEN_CHAN_LIST   = 'opcl',
    MSG_ADD_LIST_ROW     = 'adlr',
    MSG_CONTEXT_CHAN_LIST = 'cxcl',
    MSG_CLEAR_LIST_ROW   = 'cllr',
    MSG_TOGGLE_HIDE_STATUS = 'tghs',
    MSG_CONTEXT_ABOUT      = 'cxab',
    MSG_TOGGLE_AUTORECONNECT = 'tgar',
    MSG_RECONNECT_SERVER = 'rcsv',
    MSG_CONFIG_SAVE = 'cfsv',
    MSG_CONFIG_CANCEL = 'cfcn',
	MSG_CONTEXT_CONFIGURE_SERVER = 'mccs',
	MSG_SAVE_CONFIG_FILE = 'mscf',
};









// 1. DECLARE HELPER ITEM FIRST: Put ChannelRowItem at the top so the window can see it
class ChannelRowItem : public BStringItem {
public:
    ChannelRowItem(const char* channel, const char* users, const char* topic)
        : BStringItem(channel), fChannel(channel), fUsers(users), fTopic(topic) {
        fUsers << " users";
    }

    BString GetChannelName() const { return fChannel; }

    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();

        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        owner->SetFont(be_plain_font);
        
        // FIX #2: Correct native way to compute the baseline Y coordinate using GetFontHeight()
        font_height fh;
        owner->GetFontHeight(&fh);
        float baselineY = itemRect.bottom - (fh.descent + fh.leading);

        // Column A: Channel Name (Starts at pixel 10)
        owner->MovePenTo(itemRect.left + 10, baselineY);
        owner->DrawString(fChannel.String());

        // Column B: User Count (Starts at pixel 160)
        owner->MovePenTo(itemRect.left + 160, baselineY);
        owner->SetHighColor(ui_color(B_SUCCESS_COLOR)); 
        owner->DrawString(fUsers.String());

        // Column C: Topic Description (Starts at pixel 250)
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
        } else {
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
        }
        owner->MovePenTo(itemRect.left + 250, baselineY);
        
        BString truncatedTopic = fTopic;
        owner->TruncateString(&truncatedTopic, B_TRUNCATE_END, itemRect.Width() - 260);
        owner->DrawString(truncatedTopic.String());

        owner->PopState();
    }

private:
    BString fChannel;
    BString fUsers;
    BString fTopic;
};

// 2. DECLARE THE WINDOW
class IRCChannelListWindow : public BWindow {
public:
    IRCChannelListWindow(BWindow* owner, BSecureSocket* targetSocket) 
        : BWindow(BRect(150, 150, 800, 650), "Network Channel List", 
                  B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS) {
        
        fOwnerWindow = owner;
        fSocket = targetSocket;

        fListView = new BListView("chan_list_view");
        fListView->SetInvocationMessage(new BMessage('join'));
        BScrollView* scrollPane = new BScrollView("scroll_list", fListView, 0, false, true);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
            .SetInsets(5)
            .Add(scrollPane, 1.0)
            .End();

        if (fSocket != nullptr) {
            fSocket->Write("LIST\r\n", 6);
        }
    }

    void DispatchMessage(BMessage* message, BHandler* handler) override {
        if (message->what == B_MOUSE_DOWN && handler == fListView) {
            int32 buttons;
            BPoint point;
            
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_SECONDARY_MOUSE_BUTTON) {
                message->FindPoint("be:view_where", &point);
                int32 index = fListView->IndexOf(point);
                
                if (index >= 0) {
                    fListView->Select(index);
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(index));
                    
                    if (selectedItem != nullptr) {
                        BPopUpMenu* menu = new BPopUpMenu("ChannelActions", false, false);
                        menu->AddItem(new BMenuItem("Join Channel", new BMessage('join')));
                        menu->SetTargetForItems(this);
                        menu->Go(fListView->ConvertToScreen(point), true, true, true);
                        return; 
                    }
                }
            }
        }
        BWindow::DispatchMessage(message, handler);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case MSG_ADD_LIST_ROW: {
                const char* channelName;
                const char* userCount;
                const char* topic;
                
                if (message->FindString("channel", &channelName) == B_OK &&
                    message->FindString("users", &userCount) == B_OK &&
                    message->FindString("topic", &topic) == B_OK) {
                    
                    fListView->AddItem(new ChannelRowItem(channelName, userCount, topic));
                }
                break;
            }

            case 'join': {
                int32 selectedIdx = fListView->CurrentSelection();
                if (selectedIdx >= 0) {
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(selectedIdx));
                    if (selectedItem != nullptr && fSocket != nullptr) {
                        BString joinCmd;
                        joinCmd << "JOIN " << selectedItem->GetChannelName() << "\r\n";
                        fSocket->Write(joinCmd.String(), joinCmd.Length());
                        
                        if (fOwnerWindow) fOwnerWindow->Activate(true);
                        PostMessage(B_QUIT_REQUESTED); 
                    }
                }
                break;
            }

            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

    void Quit() override {
        BMessage notification('cldc');
        if (fOwnerWindow) fOwnerWindow->PostMessage(&notification);
        BWindow::Quit();
    }

private:
    BWindow*   fOwnerWindow;
    BListView* fListView;
    BSecureSocket* fSocket;
};



class ChannelTreeItem : public BStringItem {
public:
    // Accept the configuration database index pointer reference on creation
    ChannelTreeItem(const char* text, size_t serverIndex) 
        : BStringItem(text), fServerIndex(serverIndex), fHasUnread(false), fAutoJoin(false) {
        
        // Check our loaded global vector cache profiles to set the initial state flag
        BString chanName(text);
        if (fServerIndex < cfg.servers.size()) {
            for (const auto& chan : cfg.servers[fServerIndex].autojoin) {
                if (chanName.ICompare(chan.c_str()) == 0) {
                    fAutoJoin = true;
                    break;
                }
            }
        }
    }

    void SetUnread(bool unread) { fHasUnread = unread; }
    bool HasUnread() const { return fHasUnread; }
    
    void SetAutoJoin(bool autoJoin) { fAutoJoin = autoJoin; }
    bool IsAutoJoin() const { return fAutoJoin; }
    size_t GetServerIndex() const { return fServerIndex; }

    // Override the native drawing framework loop
    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        // Safe fallback cleanup context setup
        owner->PushState();

        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        // 1. DYNAMIC FONT: Read the current size from the parent view container
        BFont dynamicFont;
        owner->GetFont(&dynamicFont);

        // DYNAMIC FONT SWITCH: Apply bold formatting face rules manually on our scale
        if (fHasUnread && !IsSelected()) {
            dynamicFont.SetFace(B_BOLD_FACE);
            owner->SetHighColor(ui_color(B_LINK_TEXT_COLOR)); 
        } else {
            dynamicFont.SetFace(B_REGULAR_FACE);
        }
        owner->SetFont(&dynamicFont);

        // 2. DYNAMIC SPACING FIX: Calculate vertical baseline metrics to avoid overlapping
        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        // Indent channels to the right (e.g., 20 pixels) to distinguish them under server nodes
        owner->MovePenTo(itemRect.left + 20, textBaseline);
        
        // Rebuild dynamic text string to add trailing tag helper
        BString displayString = Text();
        if (fAutoJoin) displayString << " [A]";
        owner->DrawString(displayString.String());

        owner->PopState();
    }


private:
    size_t fServerIndex;
    bool fHasUnread;
    bool fAutoJoin;
};


class ServerTreeItem : public BStringItem {
public:
    ServerTreeItem(const char* text, size_t configIndex, bool isChannel = false) 
        : BStringItem(text), fConfigIndex(configIndex), fIsChannel(isChannel) {
        fAutoConnect = cfg.servers[fConfigIndex].autoConnect;
        fAutoReconnect = cfg.servers[fConfigIndex].autoReconnect;
    }
        
    BString GetHost() const { return BString(cfg.servers[fConfigIndex].host.c_str()); }
    uint16  GetPort() const { return cfg.servers[fConfigIndex].port; }
    BString GetNick() const { return BString(cfg.servers[fConfigIndex].nick.c_str()); }
    BString GetPass() const { return BString(cfg.servers[fConfigIndex].pass.c_str()); }
    size_t  GetIndex() const { return fConfigIndex; }
    std::vector<std::string> GetAutojoin() const { return cfg.servers[fConfigIndex].autojoin; }
    
    bool IsAutoConnect() const { return fAutoConnect; }
    void SetAutoConnect(bool autoConnect) { fAutoConnect = autoConnect; }
    bool IsAutoReconnect() const { return fAutoReconnect; }
    void SetAutoReconnect(bool autoReconnect) { fAutoReconnect = autoReconnect; }
    bool IsHideStatus() const { return fHideStatus; }
    void SetHideStatus(bool hide) { fHideStatus = hide; }
    bool IsChannel() const { return fIsChannel; }

    // FIXED: DrawItem that centers text and indents channels perfectly at any font size
    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();
        
        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        // Dynamically get the font scaling configuration applied to the parent tree
        BFont dynamicFont;
        owner->GetFont(&dynamicFont);
        owner->SetFont(&dynamicFont);

        // Dynamically calculate the vertical baseline using precise font metrics
        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        // INDENT CHANNELS: Add padding to the left if this item is a channel node
        float leftPadding = fIsChannel ? 24.0f : 4.0f;
        owner->MovePenTo(itemRect.left + leftPadding, textBaseline);
        
        BString displayString = Text();
        if (!fIsChannel) {
            if (fAutoConnect) displayString << " [AC]";
            if (fAutoReconnect) displayString << " [AR]"; 
        }
        owner->DrawString(displayString.String());

        owner->PopState();
    }

private:
    size_t fConfigIndex;
    bool fAutoConnect;
    bool fAutoReconnect;
    bool fHideStatus = false;
    bool fIsChannel; // Flag to separate drawing rules
};





class ServerConfigWindow : public BWindow {
public:
    virtual ~ServerConfigWindow() {}

    ServerConfigWindow(BWindow* parent, ServerTreeItem* item, size_t serverIdx)
        : BWindow(BRect(0, 0, 350, 320), "Server Properties", B_MODAL_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_CLOSE_ON_ESCAPE) {
        
        fParentWindow = parent;
        fItem = item;
        fServerIdx = serverIdx;

        AddToSubset(parent);
        CenterIn(parent->Frame());

        ServerConfig& srv = cfg.servers[fServerIdx];

        fNickInput = new BTextControl("nick", "Nickname:", srv.nick.c_str(), nullptr);
        fPassInput = new BTextControl("pass", "Password:", srv.pass.c_str(), nullptr);
        fPassInput->TextView()->HideTyping(true); 

        fServerListFontMenu = CreateFontMenu("Server List Font:", cfg.serverListFontSize);
        fChatLogFontMenu    = CreateFontMenu("Chat Log Font:", cfg.chatLogFontSize);
        fUserListFontMenu   = CreateFontMenu("User List Font:", cfg.userListFontSize);

// Inside ServerConfigWindow Constructor:

        fAutoConnectCheck = new BCheckBox("autoconnect", "Automatically connect at startup", nullptr);
        fAutoConnectCheck->SetValue(srv.autoConnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fAutoReconnectCheck = new BCheckBox("autoreconnect", "Automatically reconnect on drop", nullptr);
        fAutoReconnectCheck->SetValue(srv.autoReconnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fDebugEnableCheck = new BCheckBox("debug", "Enable low-level socket engine logs", nullptr);
        fDebugEnableCheck->SetValue(cfg.debugEnable ? B_CONTROL_ON : B_CONTROL_OFF);

        // NEW CODE: Instantiate the hide status check control button
        fHideStatusCheck = new BCheckBox("hidestatus", "Hide channel status messages (Joins/Parts/Quits)", nullptr);
        fHideStatusCheck->SetValue(srv.hideStatusMessages ? B_CONTROL_ON : B_CONTROL_OFF);

        BButton* cancelBtn = new BButton("cancel", "Cancel", new BMessage('cfcn'));
        BButton* saveBtn = new BButton("save", "Save", new BMessage('cfsv'));
        saveBtn->MakeDefault(true);

        // Update the layout builder chain to include the new toggle line below the font dropdown selectors
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(15)
            .AddGrid(5.0f, 5.0f)
                .Add(fNickInput->CreateLabelLayoutItem(), 0, 0)
                .Add(fNickInput->CreateTextViewLayoutItem(), 1, 0)
                .Add(fPassInput->CreateLabelLayoutItem(), 0, 1)
                .Add(fPassInput->CreateTextViewLayoutItem(), 1, 1)
                .Add(fServerListFontMenu->CreateLabelLayoutItem(), 0, 2)
                .Add(fServerListFontMenu->CreateMenuBarLayoutItem(), 1, 2)
                .Add(fChatLogFontMenu->CreateLabelLayoutItem(), 0, 3)
                .Add(fChatLogFontMenu->CreateMenuBarLayoutItem(), 1, 3)
                .Add(fUserListFontMenu->CreateLabelLayoutItem(), 0, 4)
                .Add(fUserListFontMenu->CreateMenuBarLayoutItem(), 1, 4)
            .End()
            .Add(fAutoConnectCheck)
            .Add(fAutoReconnectCheck)
            .Add(fHideStatusCheck)      // <-- NEW: Render checkbox inline horizontally
            // .Add(fDebugEnableCheck)
            .AddGlue()
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue()
                .Add(cancelBtn)
                .Add(saveBtn)
            .End();

    }

    // Moving the implementation body inline here resolves the linker vtable error
    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case 'cfcn':
                Quit();
                break;
                
	        case 'cfsv': {
            	ServerConfig& srv = cfg.servers[fServerIdx];
            	srv.nick = fNickInput->Text();
            	srv.pass = fPassInput->Text();
            	srv.autoConnect = (fAutoConnectCheck->Value() == B_CONTROL_ON);
            	srv.autoReconnect = (fAutoReconnectCheck->Value() == B_CONTROL_ON);
            	cfg.debugEnable = (fDebugEnableCheck->Value() == B_CONTROL_ON);

            	srv.hideStatusMessages = (fHideStatusCheck->Value() == B_CONTROL_ON);

            	BMenuItem* item = fServerListFontMenu->Menu()->FindMarked();
            	if (item && item->Message()) cfg.serverListFontSize = item->Message()->FindInt32("size");

            	item = fChatLogFontMenu->Menu()->FindMarked();
            	if (item && item->Message()) cfg.chatLogFontSize = item->Message()->FindInt32("size");

            	item = fUserListFontMenu->Menu()->FindMarked();
            	if (item && item->Message()) cfg.userListFontSize = item->Message()->FindInt32("size");

            	if (fItem != nullptr) {
                	fItem->SetAutoConnect(srv.autoConnect);
                	fItem->SetAutoReconnect(srv.autoReconnect);
                
                	fItem->SetHideStatus(srv.hideStatusMessages);
            	}

            	BMessage updateNotify('mscf'); 
            	if (fParentWindow) fParentWindow->PostMessage(&updateNotify);
            
            	Quit();
            	break;
        	}

            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

private:
    BWindow*            fParentWindow;
    ServerTreeItem*     fItem;
    size_t              fServerIdx;

    BTextControl*       fNickInput;
    BTextControl*       fPassInput;
    BCheckBox*          fAutoConnectCheck;
    BCheckBox*          fAutoReconnectCheck;
    BCheckBox*          fDebugEnableCheck;
	BCheckBox*          fHideStatusCheck;
    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;

    BMenuField* CreateFontMenu(const char* label, int32 currentSize) {
        BPopUpMenu* menu = new BPopUpMenu(label);
        int32 sizes[] = {9, 10, 11, 12, 14, 16, 18, 20, 24};
        
        for (int i = 0; i < 9; i++) {
            BString sizeStr;
            sizeStr << sizes[i];
            
            BMessage* msg = new BMessage('fntc');
            msg->AddInt32("size", sizes[i]);
            
            BMenuItem* item = new BMenuItem(sizeStr.String(), msg);
            if (sizes[i] == currentSize) {
                item->SetMarked(true);
            }
            menu->AddItem(item);
        }
        return new BMenuField(label, label, menu);
    }
};




class HIRCWindow : public BWindow {
public:
    HIRCWindow() : BWindow(BRect(100, 100, 900, 600), "Haiku IRC", 
                        B_DOCUMENT_WINDOW, B_QUIT_ON_WINDOW_CLOSE) {
        //@constructor
        
        // 1. Setup Channel Tree View (Left Side)
        fChannelTree = new BOutlineListView("channel_tree");
        BScrollView* channelScroll = new BScrollView("scroll_channels", fChannelTree, 0, false, true);
        
        // 2. Load Config and Populate Servers Tree ONCE
        load_config(); 
        
        fLiberaNode = nullptr;
        fOftcNode = nullptr;

        for (size_t i = 0; i < cfg.servers.size(); i++) {
            ServerTreeItem* serverNode = new ServerTreeItem(cfg.servers[i].name.c_str(), i);
            
            // Copy the loaded configuration values straight onto the UI tree items on boot!
            serverNode->SetHideStatus(cfg.servers[i].hideStatusMessages);
            
            fChannelTree->AddItem(serverNode);
            
            // Map structural lookups to our class reference tracking pointers
            if (i == 0) fLiberaNode = serverNode;
            if (i == 1) fOftcNode = serverNode;
        }

        
        // 3. Create Topic View bar Control
        fTopicView = new BTextControl("topic_view", "Topic:", "No topic set.", nullptr);
        fTopicView->TextView()->MakeEditable(false);
        
        // 4. Setup Middle & Right Components
        fChatLog = new BTextView("chat_log");
        fChatLog->MakeEditable(false);
        BScrollView* chatScroll = new BScrollView("scroll_chat", fChatLog, 0, false, true);

        fUserList = new BListView("user_list");
        BScrollView* userScroll = new BScrollView("scroll_users", fUserList, 0, false, true);

        fInputControl = new BTextControl("input", "", "", new BMessage(MSG_SEND_MESSAGE));
        fConnectButton = new BButton("connect", "Join #Haiku!", new BMessage(MSG_START_SIRC));


        // Apply saved font choices on startup
        BFont initialFont;

        // Apply Left Channel Tree Font Size
        fChannelTree->GetFont(&initialFont);
        initialFont.SetSize(cfg.serverListFontSize);
        fChannelTree->SetFont(&initialFont);

        // Apply Center Chat Text Area Font Size
        fChatLog->GetFont(&initialFont);
        initialFont.SetSize(cfg.chatLogFontSize);
        fChatLog->SetFont(&initialFont);

        // Apply Right User List Column Font Size
        fUserList->GetFont(&initialFont);
        initialFont.SetSize(cfg.userListFontSize);
        fUserList->SetFont(&initialFont);
        
        // Apply Topic View Font Size from configurations file
        fTopicView->GetFont(&initialFont);
        initialFont.SetSize(cfg.chatLogFontSize);
        fTopicView->SetFont(&initialFont);
        fTopicView->TextView()->SetFontAndColor(&initialFont, B_FONT_SIZE);


        // 5. Layout Architecture using Adjustable Split Panes
        BSplitView* mainSplitter = new BSplitView(B_HORIZONTAL, 5.0f);
        
        // Build a vertical stack container for the center chat area (Topic + Text box)
        BView* centerStackView = BLayoutBuilder::Group<>(B_VERTICAL, 5)
            .Add(fTopicView, 0.0)      // Topic row stays fixed at the top
            .Add(chatScroll, 1.0)      // Chat logs stretch vertically
            .View();

        // Pack the three components into the adjustable splitter handle container
        mainSplitter->AddChild(channelScroll, 0.25f); // Left column (weight 25%)
        mainSplitter->AddChild(centerStackView, 0.55f); // Center column (weight 55%)
        mainSplitter->AddChild(userScroll, 0.20f);    // Right column (weight 20%)

        // Attach everything to the primary window builder container shell
        BLayoutBuilder::Group<>(this, B_VERTICAL, 5)
            .SetInsets(10)
            .Add(mainSplitter, 1.0) // Splitter stretches to fill the main window space
            .AddGroup(B_HORIZONTAL, 5, 0.0) // Bottom user text entry bar row remains fixed
                .Add(fInputControl, 1.0)
                .Add(fConnectButton, 0.0)
            .End();



        // 6. Initialize State Properties
        fChannelTree->SetSelectionMessage(new BMessage('slch'));
        fActiveBufferItem = nullptr;        
        fLiberaThread = -1;
        fOftcThread = -1;
        fLiberaSocket = nullptr;
        fOftcSocket = nullptr;
        fCurrentServerNode = nullptr;
        
        // Maybe use this in the future
        // query the specific context server's nickname string
		// BString localEcho;
		// localEcho << "<" << contextServer->GetNick() << "> " << text << "\n";

        
        if (!cfg.servers.empty()) {
            fMyNick = cfg.servers[0].nick.c_str();
        } else {
            // High-precision clock backup fallback to generate matching unique IDs anywhere
            srand(static_cast<unsigned int>(real_time_clock_usecs()));
            int randomSuffix = 1000 + (rand() % 9000);
            fMyNick.SetToFormat("HaikuIRCUser%d", randomSuffix);
        }
        
        // 7. Process Automatic Startup Connections instantly
        for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
            ServerTreeItem* srvNode = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(i));
            if (srvNode != nullptr && srvNode->IsAutoConnect()) {
                ConnectToServer(srvNode);
            }
        }              
        
    }



    ~HIRCWindow() {
        delete fLiberaSocket;
        delete fOftcSocket;
        
        // Loop and free our dynamic user list pointers from memory safely
        for (auto const& [key, listPtr] : fChannelUsers) {
            delete listPtr;
        }
    }



private:
	//@Menus
    // Change parameter to BListItem* so it can accept both servers and channels
    void ShowContextMenu(BPoint screenPoint, BListItem* item) {
        if (item == nullptr) return; // Safety check
        
        // 1. Check if the right-clicked node is a child channel leaf
        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(item);
        if (chanItem != nullptr) {
            BPopUpMenu* menu = new BPopUpMenu("ChannelOptions", false, false);
            
            // Generate custom deletion code message token
            BMessage* removeMsg = new BMessage('rmch'); 
            removeMsg->AddPointer("channel_item", chanItem);
            
            menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
            menu->SetTargetForItems(this);
            menu->Go(screenPoint, true, true, true);
            return; // Exit out early since we handled the channel menu
        }

        // 2. Check if the right-clicked node is a root server container header
        ServerTreeItem* srvItem = dynamic_cast<ServerTreeItem*>(item);
        if (srvItem != nullptr) {
            BPopUpMenu* menu = new BPopUpMenu("ServerOptions", false, false);
            
            uint32 commandMsg = (srvItem == fLiberaNode) ? MSG_CONNECT_LIBERA : MSG_CONNECT_OFTC;
            menu->AddItem(new BMenuItem("Connect", new BMessage(commandMsg)));
            menu->AddSeparatorItem();

            // Toggle Auto-Connect Option
            BString toggleConnectLabel = "Auto-Connect on Startup";
            BMessage* toggleConnectMsg = new BMessage(MSG_TOGGLE_AUTOCONNECT);
            toggleConnectMsg->AddPointer("server_item", srvItem);
            BMenuItem* connectMenuItem = new BMenuItem(toggleConnectLabel.String(), toggleConnectMsg);
            if (srvItem->IsAutoConnect()) connectMenuItem->SetMarked(true);
            menu->AddItem(connectMenuItem);

            // Toggle Auto-Reconnect Option
            BString toggleReconnectLabel = srvItem->IsAutoReconnect() ? "Auto-Reconnect Enabled" : "Enable Auto-Reconnect";
            BMessage* toggleReconnectMsg = new BMessage(MSG_TOGGLE_AUTORECONNECT);
            toggleReconnectMsg->AddPointer("server_item", srvItem);
            BMenuItem* reconnectMenuItem = new BMenuItem(toggleReconnectLabel.String(), toggleReconnectMsg);
            if (srvItem->IsAutoReconnect()) reconnectMenuItem->SetMarked(true);
            menu->AddItem(reconnectMenuItem);
            
            // Status Messages Suppression Option
            BString toggleStatusLabel = srvItem->IsHideStatus() ? "Status Messages Hidden" : "Status Messages Visible";
            BMessage* toggleStatusMsg = new BMessage(MSG_TOGGLE_HIDE_STATUS);
            toggleStatusMsg->AddPointer("server_item", srvItem);
            BMenuItem* statusMenuItem = new BMenuItem(toggleStatusLabel.String(), toggleStatusMsg);
            if (srvItem->IsHideStatus()) statusMenuItem->SetMarked(true);
            menu->AddItem(statusMenuItem);
            
            // Open specific server configuration window
            menu->AddSeparatorItem();
            BMessage* configMsg = new BMessage(MSG_CONTEXT_CONFIGURE_SERVER);
            configMsg->AddPointer("server_item", srvItem);
            menu->AddItem(new BMenuItem("Configure Server...", configMsg));
            
            // Channel List Option
            menu->AddSeparatorItem();
            BMessage* listMsg = new BMessage(MSG_CONTEXT_CHAN_LIST);
            listMsg->AddPointer("server_item", srvItem);
            menu->AddItem(new BMenuItem("Channel List", listMsg));
            
            // About Option
            menu->AddSeparatorItem();
            BMessage* aboutMsg = new BMessage(MSG_CONTEXT_ABOUT);
            menu->AddItem(new BMenuItem("About...", aboutMsg));
            
            menu->SetTargetForItems(this);
            menu->Go(screenPoint, true, true, true);
        }
    }







    void ConnectToServer(ServerTreeItem* targetNode) {
        if (targetNode == fLiberaNode) {
            if (fLiberaThread >= 0) return; 
            
            // Build and insert the Server raw log node item instantly under Libera
            BStringItem* serverLogItem = new BStringItem("[Server]");
            fChannelTree->AddUnder(serverLogItem, fLiberaNode);
            fChannelTree->Expand(fLiberaNode);
            
            // If nothing is selected yet, make this new server log view active
            if (fActiveBufferItem == nullptr) fActiveBufferItem = serverLogItem;

            fTextBuffers[serverLogItem] << "Connecting to Libera Chat...\n";
            fChatLog->SetText(fTextBuffers[serverLogItem].String());

            fLiberaThread = spawn_thread(NetworkLoop, "LiberaWorker", B_NORMAL_PRIORITY, targetNode);
            if (fLiberaThread >= 0) resume_thread(fLiberaThread);
        } else if (targetNode == fOftcNode) {
            if (fOftcThread >= 0) return; 
            
            // Build and insert the Server raw log node item instantly under OFTC
            BStringItem* serverLogItem = new BStringItem("[Server]");
            fChannelTree->AddUnder(serverLogItem, fOftcNode);
            fChannelTree->Expand(fOftcNode);
            
            if (fActiveBufferItem == nullptr) fActiveBufferItem = serverLogItem;

            fTextBuffers[serverLogItem] << "Connecting to OFTC...\n";
            fChatLog->SetText(fTextBuffers[serverLogItem].String());

            fOftcThread = spawn_thread(NetworkLoop, "OFTCWorker", B_NORMAL_PRIORITY, targetNode);
            if (fOftcThread >= 0) resume_thread(fOftcThread);
        }
    }

void LogToItemBuffer(BStringItem* itemNode, BString text) {
    if (itemNode == nullptr) return;
    
    // Append text to the memory cache map
    fTextBuffers[itemNode] << text;
    
    // If the user is actively viewing this specific leaf item, update the UI live
    if (fActiveBufferItem == itemNode) {
        // 1. Move selection cursor to the absolute end of the text log
        int32 textLength = fChatLog->TextLength();
        fChatLog->Select(textLength, textLength);
        
        // 2. Prepare our custom font variations based on your config size
        BFont regularChatFont;
        fChatLog->GetFont(&regularChatFont);
        regularChatFont.SetSize(cfg.chatLogFontSize);
        regularChatFont.SetFace(B_REGULAR_FACE);

        BFont boldChatFont = regularChatFont;
        boldChatFont.SetFace(B_BOLD_FACE);

        BFont italicChatFont = regularChatFont;
        italicChatFont.SetFace(B_ITALIC_FACE);

        // Fetch current theme text color
        rgb_color textColor = {255, 255, 255, 255}; // Default white fallback
        fChatLog->GetFontAndColor(0, nullptr, &textColor);

        // 3. FIXED: Find the brackets dynamically anywhere in the text line
        int32 openingBracketIdx = text.FindFirst("<");
        int32 closingBracketIdx = text.FindFirst("> ");
        
        // Ensure both brackets exist and are in the correct sequence order
        if (openingBracketIdx != B_ERROR && closingBracketIdx != B_ERROR && openingBracketIdx < closingBracketIdx) {
            // Allocate a heap array sized for exactly 2 styling rules
            size_t arraySize = sizeof(text_run_array) + sizeof(text_run);
            text_run_array* runArray = (text_run_array*)malloc(arraySize);
            
            if (runArray != nullptr) {
                runArray->count = 2;

                // RUN 0: Everything up to the closing bracket is bold (Timestamps stay regular if before '<')
                // To keep the timestamp regular, set the offset to the opening bracket position instead:
                runArray->runs[0].offset = openingBracketIdx; 
                runArray->runs[0].font = boldChatFont;
                runArray->runs[0].color = textColor;

                // RUN 1: Style the actual message body as regular text
                int32 messageBodyStart = closingBracketIdx + 2; // Jump past "> "
                runArray->runs[1].offset = messageBodyStart;
                runArray->runs[1].font = regularChatFont;
                runArray->runs[1].color = textColor;

                fChatLog->Insert(text.String(), runArray);
                free(runArray);
            } else {
                fChatLog->Insert(text.String());
            }
        } 
        // INTERCEPTOR B: Match action string formatting (Checks anywhere or using a loose find)
        else if (text.FindFirst("* ") != B_ERROR) {
            text_run_array* actionRun = (text_run_array*)malloc(sizeof(text_run_array));
            if (actionRun != nullptr) {
                actionRun->count = 1;
                actionRun->runs[0].offset = 0;
                actionRun->runs[0].font = italicChatFont;
                actionRun->runs[0].color = {168, 101, 235, 255}; // Purple action text

                fChatLog->Insert(text.String(), actionRun);
                free(actionRun);
            } else {
                fChatLog->Insert(text.String());
            }
        }
        // INTERCEPTOR C: System logs or fallback plain strings
        else {
            text_run_array* singleRun = (text_run_array*)malloc(sizeof(text_run_array));
            if (singleRun != nullptr) {
                singleRun->count = 1;
                singleRun->runs[0].offset = 0;
                singleRun->runs[0].font = regularChatFont;
                singleRun->runs[0].color = textColor;

                fChatLog->Insert(text.String(), singleRun);
                free(singleRun);
            } else {
                fChatLog->Insert(text.String());
            }
        }
        
        // 4. Scroll down safely to show the new text
        fChatLog->ScrollToSelection();
    }
}





    BStringItem* FindServerLogNode(ServerTreeItem* serverNode) {
        if (!serverNode) return nullptr;
        // Search through the immediate children of the server node to find "[Server]"
        for (int32 i = 0; i < fChannelTree->CountItemsUnder(serverNode, true); i++) {
            BStringItem* item = dynamic_cast<BStringItem*>(fChannelTree->ItemUnderAt(serverNode, true, i));
            if (item && BString(item->Text()) == "[Server]") {
                return item;
            }
        }
        return nullptr;
    }

    ChannelTreeItem* FindChannelNode(ServerTreeItem* serverNode, BString channelName) {
        if (!serverNode) return nullptr;
        for (int32 i = 0; i < fChannelTree->CountItemsUnder(serverNode, true); i++) {
            // CAST TO OUR NEW TYPE:
            ChannelTreeItem* item = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(serverNode, true, i));
            if (item && BString(item->Text()).ICompare(channelName) == 0) {
                return item;
            }
        }
        return nullptr;
    }

    void RefreshUserListUI() {
        // Only modify the UI list if we are currently looking at an active channel leaf node
        if (fActiveBufferItem != nullptr && BString(fActiveBufferItem->Text()).StartsWith("#")) {
            fUserList->MakeEmpty();
            if (fChannelUsers.find(fActiveBufferItem) != fChannelUsers.end()) {
                BObjectList<BStringItem, true>* users = fChannelUsers[fActiveBufferItem];
                if (users != nullptr) {
                    for (int32 i = 0; i < users->CountItems(); i++) {
                        if (users->ItemAt(i) != nullptr) {
                            fUserList->AddItem(new BStringItem(users->ItemAt(i)->Text()));
                        }
                    }
                }
            }
        }
    }


    void ParseAndDisplayIRC(BString line) {   	    	
    	
        line.ReplaceAll("\r", "");
        line.ReplaceAll("\n", "");

        if (line.Length() == 0 || line.StartsWith("PING")) return;

        BString prefix = "", command = "", trailing = "";
        int32 trailingIdx = line.FindFirst(" :");
        if (trailingIdx != B_ERROR) {
            line.CopyInto(trailing, trailingIdx + 2, line.Length() - trailingIdx - 2);
            line.Truncate(trailingIdx);
        }

        int32 firstSpace = line.FindFirst(" ");
        if (line.StartsWith(":")) {
            line.CopyInto(prefix, 1, firstSpace - 1);
            line.Remove(0, firstSpace + 1);
            firstSpace = line.FindFirst(" ");
        }
        if (firstSpace != B_ERROR) {
            line.CopyInto(command, 0, firstSpace);
            line.Remove(0, firstSpace + 1);
        } else {
            command = line;
        }

        // Fallback safety check if pointer isn't initialized
        if (fCurrentServerNode == nullptr) fCurrentServerNode = fLiberaNode;

        // Catch Successful Join actions (Handles Us AND Other Users)
        if (command == "JOIN") {
            BString channelJoined = trailing.Length() > 0 ? trailing : line;
            channelJoined.ReplaceAll(" ", "");
            
            BString userWhoJoined = prefix;
            int32 exclamIdx = userWhoJoined.FindFirst("!");
            if (exclamIdx != B_ERROR) userWhoJoined.Truncate(exclamIdx);

            ChannelTreeItem* chanNode = FindChannelNode(fCurrentServerNode, channelJoined);
            if (!chanNode) {
                // If the channel node doesn't exist under this server yet, WE are the ones joining
                ChannelTreeItem* newChanNode = new ChannelTreeItem(channelJoined.String(), fCurrentServerNode->GetIndex());

                fChannelTree->AddUnder(newChanNode, fCurrentServerNode); 
                fChannelTree->Expand(fCurrentServerNode);
                
                LogToItemBuffer(newChanNode, BString("--- Joined channel ") << channelJoined << "\n");
                fChannelUsers[newChanNode] = new BObjectList<BStringItem, true>(20);
            } else {
                // Someone else joined a channel we are already in!
                if (fChannelUsers[chanNode] != nullptr) {
                    bool userExists = false;
                    for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                        BString itemTxt = fChannelUsers[chanNode]->ItemAt(i)->Text();
                        // Strip prefix for comparison matching
                        if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                        if (itemTxt == userWhoJoined) { userExists = true; break; }
                    }
                    
                    if (!userExists) {
                        fChannelUsers[chanNode]->AddItem(new BStringItem(userWhoJoined.String()));
                        
                        // Query active server node UI class wrapper instead of the global config
                        bool hideStatusOnThisServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode->IsHideStatus() : false;
                        
                        if (!hideStatusOnThisServer) {
                            LogToItemBuffer(chanNode, BString("--> ") << userWhoJoined << " has joined " << channelJoined << "\n");
                        }
                        
                        if (fActiveBufferItem == chanNode) {
                            RefreshUserListUI();
                        }
                    }

                }
            }
            return;
        }

        // NEW PROTOCOL INTERCEPTOR: Catch /LIST response lines from the network stream
        if (command == "322") { 
            // Server data line structure: <YourNick> <#Channel> <UserCount> :[Topic Text]
            // We isolate the channel name and user count fields out of the space-delimited fields
            int32 firstSpace = line.FindFirst(" ");
            if (firstSpace != B_ERROR) line.Remove(0, firstSpace + 1); // remove nickname token
            
            int32 secondSpace = line.FindFirst(" ");
            BString channelName = "";
            if (secondSpace != B_ERROR) {
                line.CopyInto(channelName, 0, secondSpace);
                line.Remove(0, secondSpace + 1);
            }
            
            int32 thirdSpace = line.FindFirst(" ");
            BString userCount = line;
            if (thirdSpace != B_ERROR) {
                userCount.Truncate(thirdSpace);
            }
            
            channelName.ReplaceAll(" ", "");
            userCount.ReplaceAll(" ", "");

            // If a channel list window is open and listening, feed the item row package over securely
            if (fActiveListWindow != nullptr) {
                BMessage* rowPackage = new BMessage(MSG_ADD_LIST_ROW);
                rowPackage->AddString("channel", channelName.String());
                rowPackage->AddString("users", userCount.String());
                rowPackage->AddString("topic", trailing.String());
                fActiveListWindow->PostMessage(rowPackage);
            }
            return;
        }


        // Live PART & QUIT Handlers (Safely removes users dynamically when they depart or disconnect)
        if (command == "PART" || command == "QUIT") {
            BString userWhoLeft = prefix;
            int32 exclamIdx = userWhoLeft.FindFirst("!");
            if (exclamIdx != B_ERROR) userWhoLeft.Truncate(exclamIdx);

            BString targetChannel = line;
            targetChannel.ReplaceAll(" ", "");

            // Look through all rooms assigned to this active network to strip the leaving nick out
            for (int32 c = 0; c < fChannelTree->CountItemsUnder(fCurrentServerNode, true); c++) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(fCurrentServerNode, true, c));
                if (!chanNode) continue;
                
                // If it's a room-specific PART, skip loops on non-matching channel strings
                if (command == "PART" && BString(chanNode->Text()) != targetChannel) continue;

                if (fChannelUsers[chanNode] != nullptr) {
                    for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                        BString itemTxt = fChannelUsers[chanNode]->ItemAt(i)->Text();
                        
                        // Explicitly clean up the prefix *before* checking against the user string
                        if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                        
                        if (itemTxt == userWhoLeft) {
                            fChannelUsers[chanNode]->RemoveItemAt(i);
                            
                            // Query your active server node class instead of the global configuration variable
                            bool hideStatusOnThisServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode->IsHideStatus() : false;
                            
                            if (!hideStatusOnThisServer) {
                                BString partNotice = (command == "PART") ? "has left" : "has quit";
                                LogToItemBuffer(chanNode, BString("<-- ") << userWhoLeft << " " << partNotice << " (" << trailing << ")\n");
                            }
                            
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                            break;
                        }



                    }
                }
            }
            return;
        }


        // Catch Server Topic Event Numbers (332 = Topic Set, 331 = No Topic Set) or live /TOPIC updates
        if (command == "332" || command == "331" || command == "TOPIC") {
            BString targetChannel = line;
            
            if (command == "332" || command == "331") {
                // Numeric lines look like: <YourNick> <#Channel> :[Topic Content if 332]
                int32 chSpace = line.FindFirst(" ");
                if (chSpace != B_ERROR) line.Remove(0, chSpace + 1);
                int32 nextSpace = line.FindFirst(" ");
                if (nextSpace != B_ERROR) line.Truncate(nextSpace);
                targetChannel = line;
            }
            targetChannel.ReplaceAll(" ", "");

            BStringItem* chanNode = FindChannelNode(fCurrentServerNode, targetChannel);
            if (chanNode) {
                BString dynamicTopic = (command == "331") ? "No topic set." : trailing;
                
                // Cache the text payload into our memory map
                fChannelTopics[chanNode] = dynamicTopic;
                
                // Live update the layout view if the user is actively viewing this room
                if (fActiveBufferItem == chanNode) {
                    fTopicView->SetText(dynamicTopic.String());
                }
                
                if (command == "332" || command == "TOPIC") {
                    LogToItemBuffer(chanNode, BString("--- Channel topic: ") << dynamicTopic << "\n");
                } else {
                    LogToItemBuffer(chanNode, "--- No channel topic is set.\n");
                }
            }
            return;
        }




        if (command == "353") { 
            int32 chIndex = line.FindLast("#");
            BString targetChannel = "";
            if (chIndex != B_ERROR) {
                line.CopyInto(targetChannel, chIndex, line.Length() - chIndex);
                targetChannel.ReplaceAll(" ", "");
            }

            BStringItem* chanNode = FindChannelNode(fCurrentServerNode, targetChannel);
            if (chanNode && fChannelUsers[chanNode] != nullptr) {
                int32 spaceIdx;
                while ((spaceIdx = trailing.FindFirst(" ")) != B_ERROR) {
                    BString singleUser;
                    trailing.CopyInto(singleUser, 0, spaceIdx);
                    trailing.Remove(0, spaceIdx + 1);
                    
                    if (singleUser.Length() > 0) {
                        // FIX HERE: Use arrow operator (->) to insert items
                        fChannelUsers[chanNode]->AddItem(new BStringItem(singleUser.String()));
                        if (fActiveBufferItem == chanNode) {
                            fUserList->AddItem(new BStringItem(singleUser.String()));
                        }
                    }
                }
                if (trailing.Length() > 0) {
                    fChannelUsers[chanNode]->AddItem(new BStringItem(trailing.String()));
                    if (fActiveBufferItem == chanNode) {
                        fUserList->AddItem(new BStringItem(trailing.String()));
                    }
                }
            }
            return;
        }


        // Dynamic NICK Modification Handlers (Saves identity renames for Us and other network users)
        if (command == "NICK") {
            BString oldNick = prefix;
            int32 exclamIdx = oldNick.FindFirst("!");
            if (exclamIdx != B_ERROR) oldNick.Truncate(exclamIdx);

            BString newNick = trailing.Length() > 0 ? trailing : line;
            newNick.ReplaceAll(" ", "");

            // If the person changing nicks is US, update our local echo variable!
            if (oldNick.ICompare(fMyNick) == 0) {
                fMyNick = newNick;
                
                // Print a clean message inside your active buffer letting you know it updated
                BString itemNotice;
                itemNotice << "--- Your nickname is now " << fMyNick << "\n";
                LogToItemBuffer(fActiveBufferItem, itemNotice);
            }

            // Sync user row indicators across all tracking pools under this server node
            for (int32 c = 0; c < fChannelTree->CountItemsUnder(fCurrentServerNode, true); c++) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(fCurrentServerNode, true, c));
                if (chanNode && fChannelUsers[chanNode] != nullptr) {
                    for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                        BString currentEntry = fChannelUsers[chanNode]->ItemAt(i)->Text();
                        BString modePrefix = "";
                        
                        if (currentEntry.StartsWith("@") || currentEntry.StartsWith("+")) {
                            currentEntry.CopyInto(modePrefix, 0, 1);
                            currentEntry.Remove(0, 1);
                        }

                        if (currentEntry == oldNick) {
                            BString updatedEntry;
                            updatedEntry << modePrefix << newNick;
                            
                            fChannelUsers[chanNode]->RemoveItemAt(i);
                            fChannelUsers[chanNode]->AddItem(new BStringItem(updatedEntry.String()), i);                            
                            BString nickNotice;
                            nickNotice << "--- " << oldNick << " is now known as " << newNick << "\n";
                            LogToItemBuffer(chanNode, nickNotice);
                            
                            // Force the UI to refresh live if the nickname changed in the active channel view
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                            break;
                        }
                    }
                }
            }
            return;
        }



        if (command == "PRIVMSG" || command == "NOTICE") {
            BString senderNick = prefix;
            int32 exclamIdx = senderNick.FindFirst("!");
            if (exclamIdx != B_ERROR) senderNick.Truncate(exclamIdx);
            
            // Extract target channel or nickname from intermediate parameter space
            int32 msgTargetSpace = line.FindFirst(" ");
            BString targetRoom = line;
            if (msgTargetSpace != B_ERROR) targetRoom.Truncate(msgTargetSpace);
            targetRoom.ReplaceAll(" ", "");
            
            // 1. Establish the destination target node item reference
            BStringItem* targetNode = nullptr;
            if (targetRoom.StartsWith("#")) {
                targetNode = FindChannelNode(fCurrentServerNode, targetRoom);
            }
            if (targetNode == nullptr) {
                targetNode = FindServerLogNode(fCurrentServerNode);
            }

            // 2. TIMING CALCULATION ENGINE
            BString timestampPrefix = "";
            bigtime_t currentTime = real_time_clock_usecs(); // Native Haiku system clock
            
            // 30 minutes expressed in microseconds (30 min * 60 sec * 1,000,000 usec)
            bigtime_t thirtyMinutesInUsecs = (bigtime_t)30 * 60 * 1000000;

            if (targetNode != nullptr) {
                // If it's a brand new channel or 30 minutes have elapsed since the last stamp
                if (fLastTimestampTime.find(targetNode) == fLastTimestampTime.end() || 
                    (currentTime - fLastTimestampTime[targetNode]) >= thirtyMinutesInUsecs) {
                    
                    fLastTimestampTime[targetNode] = currentTime;
                    time_t rawTime = (time_t)(currentTime / 1000000);
                    struct tm* timeInfo = localtime(&rawTime);
                    
                    if (timeInfo != nullptr) {
                        char timeBuffer[32]; // Hardcoded explicit size bounds array
                        strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                        timestampPrefix = timeBuffer;
                    }
                }
            }

            // Assemble final message layout string
            BString formattedMsg;
            formattedMsg << timestampPrefix;

            // NEW INTERCEPTOR: Detect and format /me CTCP ACTION strings cleanly
            if (command == "PRIVMSG" && (trailing.StartsWith("\x01" "ACTION ") || trailing.StartsWith("\1ACTION "))) { 
                       
                trailing.Remove(0, 8);
                
                // Strip out the trailing 0x01 delimiter block if it exists
                if (trailing.EndsWith("\x01") || trailing.EndsWith("\1")) {
                    trailing.Truncate(trailing.Length() - 1);
                }
                
                // Format into Action style syntax: * Nick message
                formattedMsg << "* " << senderNick << " " << trailing << "\n";
            } else {
                // Default format for standard chat room conversation
                formattedMsg << "<" << senderNick << "> " << trailing << "\n";
            }

            // AUTOMATION HOOK: Detect if NickServ is requesting authentication credentials
            if (senderNick.ICompare("NickServ") == 0 && trailing.IFindFirst("identify") != B_ERROR) {
                BSecureSocket* activeSocket = (fCurrentServerNode == fOftcNode) ? fOftcSocket : fLiberaSocket;
                
                if (activeSocket != nullptr && fCurrentServerNode != nullptr) {
                    BString savedPassword = fCurrentServerNode->GetPass();
                    
                    if (savedPassword.Length() > 0) {
                        BString autoIdentify;
                        autoIdentify << "PRIVMSG NickServ :IDENTIFY " << savedPassword << "\r\n";
                        activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                        
                        BString logNotice = "--- [Auto-Services] Identification credentials automatically sent to NickServ.\n";
                        LogToItemBuffer(FindServerLogNode(fCurrentServerNode), logNotice);
                    }
                }
            }

            // Route standard log message display & Handle Bold text flag toggles
            if (targetRoom.StartsWith("#")) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(targetNode);
                if (chanNode) {
                    // ACTION TRIGGER: If user isn't actively looking at this room node, make it bold!
                    if (fActiveBufferItem != chanNode) {
                        chanNode->SetUnread(true);
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanNode)); // Force immediate redraw
                    }
                    LogToItemBuffer(chanNode, formattedMsg);
                } else {
                    LogToItemBuffer(FindServerLogNode(fCurrentServerNode), formattedMsg);
                }
            } else {
                LogToItemBuffer(FindServerLogNode(fCurrentServerNode), formattedMsg);
            }
        }

        
        else if (trailing.Length() > 0) {
            // AUTOMATION HOOK FALLBACK: Intercept raw notices before registration fully establishes
            if (prefix.IFindFirst("NickServ") != B_ERROR && trailing.IFindFirst("identify") != B_ERROR) {
                BSecureSocket* activeSocket = (fCurrentServerNode == fOftcNode) ? fOftcSocket : fLiberaSocket;
                if (activeSocket != nullptr && fCurrentServerNode != nullptr && fCurrentServerNode->GetPass().Length() > 0) {
                    BString autoIdentify;
                    autoIdentify << "PRIVMSG NickServ :IDENTIFY " << fCurrentServerNode->GetPass() << "\r\n";
                    activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                    
                    BString logNotice = "--- [Auto-Services] Pre-registration identification sent to NickServ.\n";
                    LogToItemBuffer(FindServerLogNode(fCurrentServerNode), logNotice);
                }
            }

            // General Server Protocol output (MOTD, Notices, Connection headers) -> routes to "[Server]" tree node
            BStringItem* svrLogNode = FindServerLogNode(fCurrentServerNode);
            if (svrLogNode) {
                BString rawLog;
                rawLog << trailing << "\n";
                LogToItemBuffer(svrLogNode, rawLog);
            }
        }
    }

    static status_t NetworkLoop(void* data) {
        ServerTreeItem* targetNode = static_cast<ServerTreeItem*>(data);
        if (targetNode == nullptr) return B_ERROR;

        HIRCWindow* window = dynamic_cast<HIRCWindow*>(be_app->WindowAt(0));
        if (window == nullptr) return B_ERROR;

        BNetworkAddress address(targetNode->GetHost().String(), targetNode->GetPort());
        BSecureSocket* localSocket = new BSecureSocket();
        
        if (localSocket->Connect(address) != B_OK) {
            BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
            reply->AddString("text", BString("Connection failed to ") << targetNode->GetHost() << "\n");
            reply->AddPointer("server_node", targetNode);
            window->PostMessage(reply);
            delete localSocket;
            return B_ERROR;
        }

        if (targetNode == window->fLiberaNode) {
            window->fLiberaSocket = localSocket;
        } else {
            window->fOftcSocket = localSocket;
        }

        // 1. Dynamic Nickname Handshake Configuration
        BString nickHandshake;
        nickHandshake << "NICK " << targetNode->GetNick() << "\r\n";
        localSocket->Write(nickHandshake.String(), nickHandshake.Length());

        // 2. Dynamic User Identity Handshake Configuration
        BString userHandshake;
        userHandshake << "USER haiku 0 * :" << targetNode->GetNick() << " Client\r\n";
        localSocket->Write(userHandshake.String(), userHandshake.Length());

        // 3. Dynamic Service Password Authentication (If password exists inside JSON array data)
        if (targetNode->GetPass().Length() > 0) {
            BString passHandshake;
            passHandshake << "PASS " << targetNode->GetPass() << "\r\n";
            localSocket->Write(passHandshake.String(), passHandshake.Length());
        }
        
        // 4. Automated Services Channel Handshake Router Loop
        for (const auto& chan : targetNode->GetAutojoin()) {
            BString joinCommand;
            joinCommand << "JOIN " << chan.c_str() << "\r\n";
            localSocket->Write(joinCommand.String(), joinCommand.Length());
        }

        char buffer[512];
        ssize_t bytesRead;
        BString lineBuffer;
        
        while ((bytesRead = localSocket->Read(buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0'; 
            lineBuffer << buffer;

            int32 newlinePos;
            while ((newlinePos = lineBuffer.FindFirst("\n")) != B_ERROR) {
                BString line;
                lineBuffer.CopyInto(line, 0, newlinePos + 1);
                lineBuffer.Remove(0, newlinePos + 1);
                
                if (line.StartsWith("PING")) {
                    BString pong = line;
                    pong.Replace("PING", "PONG", 1);
                    localSocket->Write(pong.String(), pong.Length());
                    continue;
                }

                BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
                reply->AddString("text", line.String());
                reply->AddPointer("server_node", targetNode);
                window->PostMessage(reply);
            }
        }

        // 4. Thread and Socket Cleanup State Reset
        bool triggerReconnect = targetNode->IsAutoReconnect();
        
        if (targetNode == window->fLiberaNode) {
            window->fLiberaThread = -1;
            window->fLiberaSocket = nullptr;
        } else {
            window->fOftcThread = -1;
            window->fOftcSocket = nullptr;
        }
        
       // Send a post message to the window to execute a safe thread-safe reconnection loop pass
        if (triggerReconnect) {
            BMessage* reconnectMessage = new BMessage(MSG_RECONNECT_SERVER);
            reconnectMessage->AddPointer("server_item", targetNode);
            window->PostMessage(reconnectMessage);
        }

        delete localSocket;
        return B_OK;
    }

public:
    void DispatchMessage(BMessage* message, BHandler* handler) override {
        if (message->what == B_MOUSE_DOWN && handler == fChannelTree) {
            int32 buttons;
            BPoint point;
            
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_SECONDARY_MOUSE_BUTTON) {
                message->FindPoint("be:view_where", &point);
                int32 index = fChannelTree->IndexOf(point);
                
                if (index >= 0) {
                    fChannelTree->Select(index);
                    BStringItem* generalItem = dynamic_cast<BStringItem*>(fChannelTree->ItemAt(index));
                    
                    if (generalItem != nullptr) {
                        // Check if it's a root Server node item row pointer object
                        ServerTreeItem* serverItem = dynamic_cast<ServerTreeItem*>(generalItem);
                        if (serverItem != nullptr) {
                            ShowContextMenu(fChannelTree->ConvertToScreen(point), serverItem);
                            return; 
                        }
                        
                        // CHANNEL LEAF HOOK: Add the removal menu action right here
                        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(generalItem);
                        if (chanItem != nullptr) {
                            BPopUpMenu* menu = new BPopUpMenu("ChannelOptions", false, false);
                            
                            // 1. Keep your existing Auto-Join toggle option
                            BString label = chanItem->IsAutoJoin() ? "Disable Auto-Join" : "Enable Auto-Join";
                            BMessage* toggleMsg = new BMessage(MSG_TOGGLE_AUTOJOIN);
                            toggleMsg->AddPointer("chan_item", chanItem); 
                            menu->AddItem(new BMenuItem(label.String(), toggleMsg));
                            
                            // 2. FIXED: Append a separator and your new Remove Channel item
                            menu->AddSeparatorItem();
                            
                            BMessage* removeMsg = new BMessage('rmch'); // Remove Channel token
                            removeMsg->AddPointer("channel_item", chanItem);
                            menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
                            
                            menu->SetTargetForItems(this);
                            menu->Go(fChannelTree->ConvertToScreen(point), true, true, true);
                            return;
                        }
                    }
                }
            }
        }
        BWindow::DispatchMessage(message, handler);
    }


	//@messagereceived
    void MessageReceived(BMessage* message) override {
        switch (message->what) {
        	
    case 'rmch': { // Remove Channel action handler
        ChannelTreeItem* chanItem = nullptr;
        
        if (message->FindPointer("channel_item", (void**)&chanItem) == B_OK && chanItem != nullptr) {
            
            // 1. Send network message to leave communication space gracefully if socket exists
            ServerTreeItem* parentServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(chanItem));
            if (parentServer != nullptr) {
                BSecureSocket* activeSocket = (parentServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                if (activeSocket != nullptr) {
                    BString partCmd;
                    partCmd << "PART " << chanItem->Text() << " :Channel removed by user\r\n";
                    activeSocket->Write(partCmd.String(), partCmd.Length());
                }
                
                // 2. Remove the channel name from the configuration's autojoin vector array
                size_t serverIdx = parentServer->GetIndex();
                if (serverIdx < cfg.servers.size()) {
                    BString targetChanName(chanItem->Text());
                    // Remove dynamic tracking suffix text tags if present (like " [A]") before matching
                    int32 tagPos = targetChanName.FindFirst(" [");
                    if (tagPos != B_ERROR) targetChanName.Truncate(tagPos);
                    
                    auto& autojoinVec = cfg.servers[serverIdx].autojoin;
                    for (auto it = autojoinVec.begin(); it != autojoinVec.end(); ) {
                        if (targetChanName.ICompare(it->c_str()) == 0) {
                            it = autojoinVec.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }

            // 3. Wipe current chat logs from active memory views if we delete the room we are looking at
            if (fActiveBufferItem == chanItem) {
                fActiveBufferItem = nullptr;
                fChatLog->SetText("");
                fUserList->MakeEmpty();
                fTopicView->SetText("No active channel buffer targeted.");
            }
            
            fTextBuffers.erase(chanItem);
            fChannelTopics.erase(chanItem);
            
            if (fChannelUsers.find(chanItem) != fChannelUsers.end()) {
                delete fChannelUsers[chanItem];
                fChannelUsers.erase(chanItem);
            }

            // 4. Safely drop the element out of the UI tree structure entirely
            fChannelTree->RemoveItem(chanItem);
            delete chanItem; // Safe memory heap clean up management 
            
            // 5. Save settings file to disk immediately
            save_config();
        }
        break;
    }

        	
    case 'mscf': {
        save_config(); // Save new dimensions to configuration file

        // 1. Update Server List Font AND recalculate item heights (Left panel)
        BFont treeFont;
        fChannelTree->GetFont(&treeFont);
        treeFont.SetSize(cfg.serverListFontSize);
        fChannelTree->SetFont(&treeFont);

        int32 treeCount = fChannelTree->CountItems();
        for (int32 i = 0; i < treeCount; i++) {
            BListItem* item = fChannelTree->ItemAt(i);
            if (item != nullptr) {
                item->Update(fChannelTree, &treeFont);
            }
        }
        fChannelTree->InvalidateLayout();
        fChannelTree->Invalidate(); 

        // 2. FIXED: Update Topic View Bar Font Size
        BFont topicFont;
        fTopicView->GetFont(&topicFont);
        topicFont.SetSize(cfg.chatLogFontSize); // Make it match your chat log text size
        fTopicView->SetFont(&topicFont);
        fTopicView->TextView()->SetFontAndColor(&topicFont, B_FONT_SIZE); // Updates the editable region
        fTopicView->InvalidateLayout();
        fTopicView->Invalidate();

        // 3. Clear and rebuild the BTextView history buffer (Center panel)
        BFont chatFont;
        fChatLog->GetFont(&chatFont);
        chatFont.SetSize(cfg.chatLogFontSize);
        fChatLog->SetFont(&chatFont);

        BString temporaryTextBuffer(fChatLog->Text());
        text_run_array* runArray = (text_run_array*)malloc(sizeof(text_run_array));
        if (runArray != nullptr) {
            runArray->count = 1;
            runArray->runs[0].offset = 0;
            runArray->runs[0].font = chatFont;
            runArray->runs[0].color = {255, 255, 255, 255}; 
            fChatLog->SetText("");
            fChatLog->Insert(temporaryTextBuffer.String(), runArray);
            free(runArray);
        }
        fChatLog->Invalidate();

        // 4. Update User List Font AND recalculate item heights (Right panel)
        BFont userFont;
        fUserList->GetFont(&userFont);
        userFont.SetSize(cfg.userListFontSize);
        fUserList->SetFont(&userFont);

        int32 userCount = fUserList->CountItems();
        for (int32 i = 0; i < userCount; i++) {
            BListItem* item = fUserList->ItemAt(i);
            if (item != nullptr) {
                item->Update(fUserList, &userFont);
            }
        }
        fUserList->InvalidateLayout();
        fUserList->Invalidate();

        break;
    }




    case MSG_CONTEXT_CONFIGURE_SERVER: {
        ServerTreeItem* item = nullptr;
        if (message->FindPointer("server_item", (void**)&item) == B_OK && item != nullptr) {
            
            // Loop through your configuration vectors to map the correct active vector index block
            size_t matchingIndex = 0;
            bool found = false;
            for (size_t i = 0; i < cfg.servers.size(); i++) {
                // Check matching pointer trackers or structural node names
                if ((item == fLiberaNode && i == 0) || (item == fOftcNode && i == 1)) {
                    matchingIndex = i;
                    found = true;
                    break;
                }
            }
            
            // Fallback match check by string text comparison if custom server added
            if(!found) {
                for (size_t i = 0; i < cfg.servers.size(); i++) {
                    if (cfg.servers[i].name == item->Text()) {
                        matchingIndex = i;
                        found = true;
                        break;
                    }
                }
            }

            // Spawn the custom configuration layout panel cleanly
            ServerConfigWindow* configWin = new ServerConfigWindow(this, item, matchingIndex);
            configWin->Show();
        }
        break;
    }

/*
    case MSG_SAVE_CONFIG_FILE: {
        // Trigger your configuration save code to push updates straight to hard disk
        save_config();
        
        // Push safe feedback notification directly into the active viewport
        LogToItemBuffer(fActiveBufferItem, "System: Configuration updates successfully written to disk.\n");
        break;
    }
*/


            case MSG_RECONNECT_SERVER: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        BString textNotice;
                        textNotice << "--- [Auto-Reconnect] Connection lost. Attempting to reconnect to " << serverItem->Text() << "...\n";
                        LogToItemBuffer(FindServerLogNode(serverItem), textNotice);
                        
                        // Fire up the native connection runner cleanly
                        ConnectToServer(serverItem);
                    }
                }
                break;
            }


            case MSG_TOGGLE_AUTORECONNECT: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        bool newState = !serverItem->IsAutoReconnect();
                        serverItem->SetAutoReconnect(newState);
                        cfg.servers[idx].autoReconnect = newState;
                        
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(serverItem));
                    }
                }
                break;
            }


            case MSG_CONTEXT_ABOUT: {
                // Formulate the informational message body text
                BString aboutText;
                aboutText << "Haiku IRC Client (hirc)\n"
                          << "Version 0.0.1\n"
                		  << "By Kris Beazley \"ablyss\"\n"
                		  << "Copyright 2026 The MIT License\n\n"

                          << "A lightweight, multi-server IRC client built natively "
                          << "for the Haiku Operating System utilizing BSplitView layout architectures.\n\n"
                          << "Features JSON configuration saving, automated services identification, "
                          << "custom spreadsheet channel list navigators, and dynamic dark mode adaptability.";

                // Instantiate a native Haiku informational dialog modal box
                BAlert* aboutAlert = new BAlert("About hirc", aboutText.String(), "OK", 
                                                nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
                
                // Instruct the alert modal box to float asynchronously so it doesn't freeze the main app loop
                aboutAlert->Go();
                break;
            }

        	
		    case MSG_TOGGLE_HIDE_STATUS: {
        		ServerTreeItem* item = nullptr;
        		if (message->FindPointer("server_item", (void**)&item) == B_OK && item != nullptr) {
            		// Flip the state inside the UI element
            		bool newStatus = !item->IsHideStatus();
            		item->SetHideStatus(newStatus);
            
            		BString itemText(item->Text());

            		// Map the item to our active config vector slot
            		for (size_t i = 0; i < cfg.servers.size(); i++) {
               			// Use StartsWith to completely ignore any dynamic trailing tags like [AC] or [AR]
                		if ((item == fLiberaNode && i == 0) || 
                    		(item == fOftcNode && i == 1) || 
                    		(itemText.StartsWith(cfg.servers[i].name.c_str()))) {
                    
                    		cfg.servers[i].hideStatusMessages = newStatus;
                    		break;
                		}
            		}
            		save_config(); 
        		}
        		break;
    		}


        	
        	
            case MSG_CONTEXT_CHAN_LIST: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        // Pick the active network socket matching the right-clicked server node
                        BSecureSocket* activeSocket = (serverItem == fOftcNode) ? fOftcSocket : fLiberaSocket;
                        
                        if (activeSocket != nullptr) {
                            // Launch or activate the float list view pop-up window
                            if (fActiveListWindow == nullptr) {
                                fActiveListWindow = new IRCChannelListWindow(this, activeSocket);
                                fActiveListWindow->Show();
                            } else {
                                fActiveListWindow->Activate(true);
                            }
                        } else {
                            // Fallback warning if the user attempts to view channels while completely offline
                            BString warning = "System Error: You must connect to this server first before requesting a channel list.\n";
                            LogToItemBuffer(fActiveBufferItem, warning);
                        }
                    }
                }
                break;
            }

        	
           case 'cldc':
                fActiveListWindow = nullptr;
                break;

        	
            case MSG_TOGGLE_AUTOCONNECT: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        // Invert state value
                        bool newState = !serverItem->IsAutoConnect();
                        serverItem->SetAutoConnect(newState);
                        cfg.servers[idx].autoConnect = newState;
                        
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(serverItem));
                    }
                }
                break;
            }
            
            case B_COLORS_UPDATED: {
                // 1. Grab the fresh system palette colors that were just chosen
                rgb_color newBgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
                rgb_color newTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);

                // 2. Re-apply the raw color configuration vectors to the canvas container
                fChatLog->SetViewColor(newBgColor);
                fChatLog->SetLowColor(newBgColor);
                
                // 3. Force all existing text in the buffer log to match the new color instantly
                fChatLog->SetFontAndColor(be_plain_font, B_FONT_ALL, &newTextColor);
                
                // 4. Update the Topic input bar at the top to match as well
                fTopicView->TextView()->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
                fTopicView->TextView()->SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
                
                rgb_color panelTextColor = ui_color(B_PANEL_TEXT_COLOR);
                fTopicView->TextView()->SetFontAndColor(be_plain_font, B_FONT_ALL, &panelTextColor);

                // 5. Force an immediate pixel invalidate re-draw cycle across everything
                fChatLog->Invalidate();
                fTopicView->Invalidate();
                this->InvalidateLayout(true);
                break;
            }

        	
            case MSG_TOGGLE_AUTOJOIN: {
                void* ptr;
                if (message->FindPointer("chan_item", &ptr) == B_OK) {
                    ChannelTreeItem* chanItem = static_cast<ChannelTreeItem*>(ptr);
                    if (chanItem != nullptr) {
                        size_t srvIdx = chanItem->GetServerIndex();
                        std::string targetChan = chanItem->Text();
                        
                        if (chanItem->IsAutoJoin()) {
                            // Deactivate: Remove the string leaf value from our configuration vector maps array data
                            auto& vec = cfg.servers[srvIdx].autojoin;
                            vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                            chanItem->SetAutoJoin(false);
                        } else {
                            // Activate: Append string into configuration profile
                            cfg.servers[srvIdx].autojoin.push_back(targetChan);
                            chanItem->SetAutoJoin(true);
                        }
                        
                        // Flush the changes instantly onto the hard disk and force row graphics updates
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanItem));
                    }
                }
                break;
            }

        	
            case 'slch': {
                int32 selectedIdx = fChannelTree->CurrentSelection();
                if (selectedIdx >= 0) {
                    BStringItem* selectedItem = dynamic_cast<BStringItem*>(fChannelTree->ItemAt(selectedIdx));
                    if (selectedItem != nullptr && selectedItem != fActiveBufferItem) {
                        
                        // 1. Cache current chat history log text back to memory
                        if (fActiveBufferItem != nullptr) {
                            fTextBuffers[fActiveBufferItem] = fChatLog->Text();
                        }
                        
                        fActiveBufferItem = selectedItem;
                        
                        // ACTION RESET: Check if the clicked item is a custom channel leaf node
                        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(selectedItem);
                        if (chanItem != nullptr && chanItem->HasUnread()) {
                            chanItem->SetUnread(false);
                            fChannelTree->InvalidateItem(selectedIdx); // Force view to drop bold look instantly
                        }
                        
                        // 2. Clear visual modules
                        fChatLog->SetText("");
                        fUserList->MakeEmpty();
                        fTopicView->SetText("No topic set.");

                        BString itemText(selectedItem->Text());
                        if (itemText == "[Server]") {
                            fTopicView->SetText("Network connection logs status feed.");
                        } else if (itemText.StartsWith("#")) {
                            // Restore active channel topic text if cached
                            if (fChannelTopics.find(fActiveBufferItem) != fChannelTopics.end()) {
                                fTopicView->SetText(fChannelTopics[fActiveBufferItem].String());
                            }
                            
                            // Restore the active user list including their channel modes!
                            if (fChannelUsers.find(fActiveBufferItem) != fChannelUsers.end()) {
                                BObjectList<BStringItem, true>* users = fChannelUsers[fActiveBufferItem];
                                
                                if (users != nullptr) {
                                    // Set up a baseline user font size tracker
                                    BFont userListFont;
                                    fUserList->GetFont(&userListFont);
                                    userListFont.SetSize(cfg.userListFontSize);

                                    for (int32 i = 0; i < users->CountItems(); i++) {
                                        if (users->ItemAt(i) != nullptr) {
                                            BStringItem* newUserItem = new BStringItem(users->ItemAt(i)->Text());
                                            fUserList->AddItem(newUserItem);
                                            
                                            // FIXED: Force individual user rows to calculate heights at your custom scale
                                            newUserItem->Update(fUserList, &userListFont);
                                        }
                                    }
                                    fUserList->InvalidateLayout();
                                    fUserList->Invalidate();
                                }
                            }
                        }

                        // 3. FIXED: Re-populate visible chat log panel using your configuration dimensions
                        if (fTextBuffers.find(fActiveBufferItem) != fTextBuffers.end()) {
                            
                            // Grab current container font parameters and force your layout configuration sizing
                            BFont customChatFont;
                            fChatLog->GetFont(&customChatFont);
                            customChatFont.SetSize(cfg.chatLogFontSize);
                            
                            rgb_color currentThemeTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);
                            
                            // DO NOT use be_plain_font here; pass your explicitly sized customChatFont
                            fChatLog->SetFontAndColor(&customChatFont, B_FONT_ALL, &currentThemeTextColor);
                            
                            // Construct a heap-allocated run array map to uniformly enforce the size across history data
                            text_run_array* historyRuns = (text_run_array*)malloc(sizeof(text_run_array));
                            if (historyRuns != nullptr) {
                                historyRuns->count = 1;
                                historyRuns->runs[0].offset = 0;
                                historyRuns->runs[0].font = customChatFont;
                                historyRuns->runs[0].color = currentThemeTextColor;

                                fChatLog->SetText("");
                                fChatLog->Insert(fTextBuffers[fActiveBufferItem].String(), historyRuns);
                                free(historyRuns);
                            } else {
                                fChatLog->SetText(fTextBuffers[fActiveBufferItem].String());
                            }
                        } else {
                            fChatLog->SetText("");
                        }
                        
                        // Force the scrollbar viewport directly back down to the bottom
                        int32 newLength = fChatLog->TextLength();
                        fChatLog->Select(newLength, newLength);
                        fChatLog->ScrollToSelection();

                    }
                }
                break;
            }



        	
        	
            case MSG_CONNECT_LIBERA:
                ConnectToServer(fLiberaNode);
                break;

            case MSG_CONNECT_OFTC:
                ConnectToServer(fOftcNode);
                break;

            case MSG_START_SIRC:
                ConnectToServer(fOftcNode); 
                break;

            case MSG_SEND_MESSAGE: {
                BString text = fInputControl->Text();
                if (text.Length() > 0) {
                    BString activeTarget = "";
                    ServerTreeItem* contextServer = nullptr;
                    
                    int32 selectedIdx = fChannelTree->CurrentSelection();
                    if (selectedIdx >= 0) {
                        BStringItem* selectedItem = dynamic_cast<BStringItem*>(fChannelTree->ItemAt(selectedIdx));
                        if (selectedItem != nullptr) {
                            BString itemText(selectedItem->Text());
                            if (itemText.StartsWith("#") || itemText == "[Server]") {
                                if (itemText.StartsWith("#")) {
                                    activeTarget = itemText;
                                }
                                contextServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(selectedItem));
                            } else {
                                contextServer = static_cast<ServerTreeItem*>(selectedItem);
                            }
                        }
                    }

                    if (contextServer == nullptr) contextServer = fLiberaNode;
                    BSecureSocket* activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;

                    if (activeSocket != nullptr) {
                        BString rawPayload;
                        
                        if (text.StartsWith("/")) {
                            BString commandLine = text;
                            commandLine.Remove(0, 1); // strip the '/'
                            
                            // NEW INTERCEPTOR: Clear current buffer view and cache
                            if (commandLine.ICompare("clear", 5) == 0) {
                                if (fActiveBufferItem != nullptr) {
                                    fTextBuffers[fActiveBufferItem] = "";
                                    fChatLog->SetText("");
                                }
                            }
                            // NEW INTERCEPTOR: Process outgoing /me actions
                            else if (commandLine.ICompare("me ", 3) == 0) {
                                commandLine.Remove(0, 3); // strip "me " keyword and space
                                
                                if (activeTarget.StartsWith("#") && commandLine.Length() > 0) {
                                    // 1. Format into official outgoing IRC CTCP protocol format
                                    // Separated to prevent hex sequence range warnings!
                                    rawPayload << "PRIVMSG " << activeTarget << " :" "\x01" "ACTION " << commandLine << "\x01\r\n";
                                    
                                    // 2. Generate matching time prefix for consistent log rules
                                    BString timestampPrefix = "";
                                    bigtime_t currentTime = real_time_clock_usecs();
                                    time_t rawTime = (time_t)(currentTime / 1000000);
                                    struct tm* timeInfo = localtime(&rawTime);
                                    if (timeInfo != nullptr) {
                                        char timeBuffer[32];
                                        strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                                        timestampPrefix = timeBuffer;
                                    }

                                    // 3. Render your action locally
                                    BString echoStr;
                                    echoStr << timestampPrefix << "* " << fMyNick << " " << commandLine << "\n";
                                    LogToItemBuffer(fActiveBufferItem, echoStr);
                                } else {
                                    BString warning = "System Error: You can only use /me actions inside a channel room leaf node.\n";
                                    LogToItemBuffer(fActiveBufferItem, warning);
                                }
                            }
                            // Explicitly intercept "/msg <target> <message>" 
                            else if (commandLine.ICompare("msg ", 4) == 0) {
                                commandLine.Remove(0, 4); // strip "msg " keyword
                                int32 firstSpace = commandLine.FindFirst(" ");
                                
                                if (firstSpace != B_ERROR) {
                                    BString dmTarget, msgBody;
                                    commandLine.CopyInto(dmTarget, 0, firstSpace);
                                    commandLine.CopyInto(msgBody, firstSpace + 1, commandLine.Length() - (firstSpace + 1));
                                    
                                    // Translate into official IRC protocol syntax
                                    rawPayload << "PRIVMSG " << dmTarget << " :" << msgBody << "\r\n";
                                    
                                    // Echo the outgoing whisper locally into your active window
                                    BString echoStr;
                                    echoStr << "-> To " << dmTarget << ": " << msgBody << "\n";
                                    LogToItemBuffer(fActiveBufferItem, echoStr);
                                }
                            } else if (commandLine.ICompare("list", 4) == 0) {
                                // NEW INTERCEPTOR: Open up global network channels pop-up directory
                                if (fActiveListWindow == nullptr) {
                                    fActiveListWindow = new IRCChannelListWindow(this, activeSocket);
                                    fActiveListWindow->Show();
                                } else {
                                    fActiveListWindow->Activate(true);
                                }
                            } else {
                                // Default pass-through fallback for normal commands like /JOIN or /NICK
                                rawPayload << commandLine << "\r\n";
                            }
                        } else {
                            // Only send a PRIVMSG if we are sitting in a valid chat room leaf node
                            if (activeTarget.StartsWith("#")) {
                                rawPayload << "PRIVMSG " << activeTarget << " :" << text << "\r\n";
                                
                                BString localEcho;
                                localEcho << "<" << fMyNick << "> " << text << "\n";
                                LogToItemBuffer(fActiveBufferItem, localEcho);
                            } else {
                                BString warning = "System Error: Use slash commands (like /JOIN) when typing inside the server status log.\n";
                                LogToItemBuffer(fActiveBufferItem, warning);
                            }
                        }

                        if (rawPayload.Length() > 0) {
                            activeSocket->Write(rawPayload.String(), rawPayload.Length());
                        }
                    }
                }
                fInputControl->SetText("");
                fInputControl->MakeFocus(true);
                break;
            }





            case MSG_IRC_RECEIVED: {
                BString rawLine;
                void* nodePtr;
                if (message->FindString("text", &rawLine) == B_OK) {
                    if (message->FindPointer("server_node", &nodePtr) == B_OK) {
                        fCurrentServerNode = static_cast<ServerTreeItem*>(nodePtr);
                    }
                    ParseAndDisplayIRC(rawLine);
                }
                break;
            }

            default:
                BWindow::MessageReceived(message);
                break;
        }
    }


private:

    BOutlineListView* fChannelTree;
    ServerTreeItem*   fLiberaNode;
    ServerTreeItem*   fOftcNode;
    ServerTreeItem*   fCurrentServerNode; 
    BString           fMyNick;
    BStringItem*      fActiveBufferItem;
    
    std::map<BStringItem*, BString> fTextBuffers;
    std::map<BStringItem*, BString> fChannelTopics;
    std::map<BStringItem*, BObjectList<BStringItem, true>*> fChannelUsers;
    std::map<BStringItem*, bigtime_t> fLastTimestampTime;
      
	BTextControl*     fTopicView;
    BTextView*        fChatLog;
    BListView*        fUserList;
    BTextControl*     fInputControl;
    BButton*          fConnectButton;
    
    thread_id         fLiberaThread;
    thread_id         fOftcThread;
    BSecureSocket*    fLiberaSocket;
    BSecureSocket*    fOftcSocket;
    
    IRCChannelListWindow* fActiveListWindow;

    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;

    BMenuField*         CreateFontMenu(const char* label, int32 currentSize);
	BCheckBox*          fHideStatusCheck;
	
}; 

class HIRC : public BApplication {
public:
    HIRC() : BApplication("application/x-vnd.HIRC") {}
    void ReadyToRun() override {
        HIRCWindow* window = new HIRCWindow();
        window->Show();
    }
};

int main() {
    HIRC app;
    app.Run();
    return 0;
}
