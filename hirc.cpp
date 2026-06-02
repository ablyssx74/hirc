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
    std::vector<ServerConfig> customServers; 
    int32 serverListFontSize = 12;
    int32 chatLogFontSize = 12;
    int32 userListFontSize = 12;
    std::string quitMessage = "Haiku IRC Client (hirc)"; 
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
    j["quitMessage"] = cfg.quitMessage;
    
    // 1. Save standard/default servers
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

    // 2. Save custom servers into their own isolated JSON array
    json customServerArray = json::array();
    for (const auto& srv : cfg.customServers) {
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
        customServerArray.push_back(s);
    }
    j["custom_servers"] = customServerArray; // <-- Distinct JSON key

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
 
    cfg.servers.clear(); 
    cfg.customServers.clear();
	
    BPath path;
    bool mustSaveDefaults = false;

    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("hirc/hircConfig.txt");
        std::ifstream infile(path.Path());        
        if (infile.is_open()) {
            try {
                json j = json::parse(infile);
                cfg.quitMessage = j.value("quitMessage", "Haiku HIRC Client");
                cfg.debugEnable = j.value("debugEnable", false);             
                cfg.serverListFontSize = j.value("serverListFontSize", (int32)12);
                cfg.chatLogFontSize    = j.value("chatLogFontSize", (int32)12);
                cfg.userListFontSize   = j.value("userListFontSize", (int32)12);  
                
                // Parse standard servers array
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
                        cfg.servers.push_back(srv);
                    }
                }

                // Parse the custom servers array safely
                if (j.contains("custom_servers") && j["custom_servers"].is_array()) {
                    for (const auto& s : j["custom_servers"]) {
                        ServerConfig srv;
                        srv.name = s.value("name", "Custom Server");
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
                        cfg.customServers.push_back(srv);
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
        srand(static_cast<unsigned int>(real_time_clock_usecs()));
        int randomSuffix = 1000 + (rand() % 9000);
        BString dynamicNick;
        dynamicNick << "HaikuIRCUser" << randomSuffix;

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
	MSG_CONNECT_CUSTOM_SERVER = 'cncs', 
	MSG_ADD_CUSTOM_SERVER_SUBMIT = 'acss',
};






class AddServerWindow : public BWindow {
public:
    AddServerWindow(BWindow* targetWindow) 
        : BWindow(BRect(0, 0, 350, 250), "Add Custom Server", 
                  B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS) {
        
        fTarget = targetWindow;

        // 1. Initialize modern, auto-aligning input layout fields
        fNameField = new BTextControl("name", "Network Name:", "My IRC Server", nullptr);
        fHostField = new BTextControl("host", "Server Host:", "://example.com", nullptr);
        fPortField = new BTextControl("port", "Port:", "6697", nullptr);
        fNickField = new BTextControl("nick", "Nickname:", "HaikuUser", nullptr);
        fPassField = new BTextControl("pass", "Password (Optional):", "", nullptr);
        
        // Hide password characters automatically
        fPassField->TextView()->HideTyping(true);

        // 2. Control Form Buttons
        fCancelButton = new BButton("cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        fSaveButton   = new BButton("save", "Add Server", new BMessage(MSG_ADD_CUSTOM_SERVER_SUBMIT));
        
        // Set the save button as the default highlight action on hitting 'Enter' key
        fSaveButton->MakeDefault(true);

        // 3. Build UI Architecture via Group Layouts
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(12)
            .AddGrid(5.0f, 5.0f) // Vertically aligns the colons of the text inputs perfectly
                .Add(fNameField->CreateLabelLayoutItem(), 0, 0)
                .Add(fNameField->CreateTextViewLayoutItem(), 1, 0)
                
                .Add(fHostField->CreateLabelLayoutItem(), 0, 1)
                .Add(fHostField->CreateTextViewLayoutItem(), 1, 1)
                
                .Add(fPortField->CreateLabelLayoutItem(), 0, 2)
                .Add(fPortField->CreateTextViewLayoutItem(), 1, 2)
                
                .Add(fNickField->CreateLabelLayoutItem(), 0, 3)
                .Add(fNickField->CreateTextViewLayoutItem(), 1, 3)
                
                .Add(fPassField->CreateLabelLayoutItem(), 0, 4)
                .Add(fPassField->CreateTextViewLayoutItem(), 1, 4)
            .End()
            .AddGlue() // Pushes inputs up and action control buttons down
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue() // Right-aligns buttons cleanly
                .Add(fCancelButton)
                .Add(fSaveButton)
            .End();

        // 4. Center this modal dynamically directly over the main application
        CenterIn(targetWindow->Frame());
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case MSG_ADD_CUSTOM_SERVER_SUBMIT: {
                // Perform quick sanitization constraints check
                if (strlen(fNameField->Text()) == 0 || strlen(fHostField->Text()) == 0) {
                    return; 
                }

                // Pack everything securely into a payload carrier message
                BMessage reply(MSG_ADD_CUSTOM_SERVER_SUBMIT);
                reply.AddString("name", fNameField->Text());
                reply.AddString("host", fHostField->Text());
                reply.AddInt32("port", atoi(fPortField->Text()));
                reply.AddString("nick", fNickField->Text());
                reply.AddString("pass", fPassField->Text());

                // Post asynchronous message back to the main UI frame
                fTarget->PostMessage(&reply);
                
                // Close dialog
                PostMessage(B_QUIT_REQUESTED);
                break;
            }
            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

private:
    BWindow*       fTarget;
    BTextControl*  fNameField;
    BTextControl*  fHostField;
    BTextControl*  fPortField;
    BTextControl*  fNickField;
    BTextControl*  fPassField;
    BButton*       fCancelButton;
    BButton*       fSaveButton;
};




// 1. DECLARE HELPER ITEM FIRST: Put ChannelRowItem at the top so the window can see it
class ChannelRowItem : public BStringItem {
public:
    ChannelRowItem(const char* channel, const char* users, const char* topic)
        : BStringItem(channel), fChannel(channel), fUsers(users), fTopic(topic) {
        fRawUserCount = atoi(users); 	
        fUsers << " users";
    }

    BString GetChannelName() const { return fChannel; }
    int32   GetUserCount() const { return fRawUserCount; } 

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

        // UPDATED: Read the true runtime font scale from the parent view context container
        BFont dynamicListFont;
        owner->GetFont(&dynamicListFont);
        owner->SetFont(&dynamicListFont);
        
        // FIX #2: Correct native way to compute the baseline Y coordinate using GetFontHeight()
        font_height fh;
        dynamicListFont.GetHeight(&fh); // Compute baseline using the dynamic font metrics
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
    int32   fRawUserCount;
};


// Sorts items so that the highest user count bubbles up to the top
// Use const void* to match the native BListView API
static int SortChannelsByUsers(const void* first, const void* second) {
    // 1. Cast the generic raw pointers to constant BListItem pointers safely
    const BListItem* itemPtrA = *static_cast<const BListItem* const*>(first);
    const BListItem* itemPtrB = *static_cast<const BListItem* const*>(second);

    // 2. Perform dynamic_cast checks to verify these are indeed ChannelRowItems
    const ChannelRowItem* itemA = dynamic_cast<const ChannelRowItem*>(itemPtrA);
    const ChannelRowItem* itemB = dynamic_cast<const ChannelRowItem*>(itemPtrB);

    if (itemA == nullptr || itemB == nullptr) return 0;

    // Descending order sort (highest count first)
    if (itemA->GetUserCount() > itemB->GetUserCount()) return -1;
    if (itemA->GetUserCount() < itemB->GetUserCount()) return 1;
    return 0;
}



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

    // NEW: Public getter required by the '322' protocol parser block
    // Ensures incoming channel data packages route exclusively to their true parent socket
    BSecureSocket* GetTargetSocket() const { return fSocket; }

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
            
            		// NEW SORT TRIGGER: Automatically re-sorts the rows live on screen!
            		fListView->SortItems(SortChannelsByUsers); 
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
        // FIXED: Loop and explicitly free all custom ChannelRowItem allocations 
        // out of the heap pool to prevent huge memory leaks when closing long directory list passes
        while (fListView->CountItems() > 0) {
            BListItem* item = fListView->RemoveItem((int32)0);
            delete item;
        }

        BMessage notification('cldc'); // Notify parent window fActiveListWindow is dead
        if (fOwnerWindow) fOwnerWindow->PostMessage(&notification);
        BWindow::Quit();
    }

private:
    BWindow*       fOwnerWindow;
    BListView*     fListView;
    BSecureSocket* fSocket;
};



class ChannelTreeItem : public BStringItem {
public:
    // UPDATED: Accept an additional bool isCustom parameter (defaults to false)
    ChannelTreeItem(const char* text, size_t serverIndex, bool isCustom = false) 
        : BStringItem(text), fServerIndex(serverIndex), fIsCustom(isCustom), fHasUnread(false), fAutoJoin(false) {
        
        BString chanName(text);
        
        // UPDATED: Route the initial autojoin check to the correct config vector array
        if (fIsCustom) {
            if (fServerIndex < cfg.customServers.size()) {
                for (const auto& chan : cfg.customServers[fServerIndex].autojoin) {
                    if (chanName.ICompare(chan.c_str()) == 0) {
                        fAutoJoin = true;
                        break;
                    }
                }
            }
        } else {
            if (fServerIndex < cfg.servers.size()) {
                for (const auto& chan : cfg.servers[fServerIndex].autojoin) {
                    if (chanName.ICompare(chan.c_str()) == 0) {
                        fAutoJoin = true;
                        break;
                    }
                }
            }
        }
    }

    void SetUnread(bool unread) { fHasUnread = unread; }
    bool HasUnread() const { return fHasUnread; }
    
    void SetAutoJoin(bool autoJoin) { fAutoJoin = autoJoin; }
    bool IsAutoJoin() const { return fAutoJoin; }
    size_t GetServerIndex() const { return fServerIndex; }
    bool   IsCustom() const { return fIsCustom; } // NEW: Public getter to query custom flag

    // Override the native drawing framework loop
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

        BFont dynamicFont;
        owner->GetFont(&dynamicFont);

        if (fHasUnread && !IsSelected()) {
            dynamicFont.SetFace(B_BOLD_FACE);
            owner->SetHighColor(ui_color(B_LINK_TEXT_COLOR)); 
        } else {
            dynamicFont.SetFace(B_REGULAR_FACE);
        }
        owner->SetFont(&dynamicFont);

        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        owner->MovePenTo(itemRect.left + 20, textBaseline);
        
        BString displayString = Text();
        if (fAutoJoin) displayString << " [A]";
        owner->DrawString(displayString.String());

        owner->PopState();
    }

private:
    size_t fServerIndex;
    bool   fIsCustom; // NEW: Track vector classification path inside instance memory
    bool   fHasUnread;
    bool   fAutoJoin;
};


class ServerTreeItem : public BStringItem {
public:
    ServerTreeItem(const char* text, size_t configIndex, bool isChannel = false, bool isCustom = false) 
        : BStringItem(text), fConfigIndex(configIndex), fIsChannel(isChannel), fIsCustom(isCustom) {
        
        // Safety bounds check to pull structural configs securely
        if (fIsCustom) {
            if (fConfigIndex < cfg.customServers.size()) {
                fAutoConnect = cfg.customServers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.customServers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.customServers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
            }
        } else {
            if (fConfigIndex < cfg.servers.size()) {
                fAutoConnect = cfg.servers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.servers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.servers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
            }
        }
    }
        
    BString GetHost() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].host.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].host.c_str());
        return BString("");
    }

    uint16 GetPort() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return cfg.customServers[fConfigIndex].port;
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return cfg.servers[fConfigIndex].port;
        return 6697;
    }

    BString GetNick() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].nick.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].nick.c_str());
        return BString("HaikuUser");
    }

    BString GetPass() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].pass.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].pass.c_str());
        return BString("");
    }

    std::vector<std::string> GetAutojoin() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return cfg.customServers[fConfigIndex].autojoin;
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return cfg.servers[fConfigIndex].autojoin;
        return std::vector<std::string>();
    }

    size_t GetIndex() const { return fConfigIndex; }
    bool   IsCustom() const { return fIsCustom; }
    bool   IsChannel() const { return fIsChannel; }
    
    bool IsAutoConnect() const { return fAutoConnect; }
    void SetAutoConnect(bool autoConnect) { fAutoConnect = autoConnect; }
    bool IsAutoReconnect() const { return fAutoReconnect; }
    void SetAutoReconnect(bool autoReconnect) { fAutoReconnect = autoReconnect; }
    bool IsHideStatus() const { return fHideStatus; }
    void SetHideStatus(bool hide) { fHideStatus = hide; }
	void SetIndex(size_t idx) { fConfigIndex = idx; }
	
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

        BFont dynamicFont;
        owner->GetFont(&dynamicFont);
        owner->SetFont(&dynamicFont);

        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
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
    bool fIsChannel; 
    bool fIsCustom; // <-- Tracks whether this item references custom array configs
};





class ServerConfigWindow : public BWindow {
public:
    virtual ~ServerConfigWindow() {}

    // UPDATED: Added bool isCustom = false parameter to the constructor signature
    ServerConfigWindow(BWindow* parent, ServerTreeItem* item, size_t serverIdx, bool isCustom = false)
        // Pass an empty dummy size; B_AUTO_UPDATE_SIZE_LIMITS will force it to fit perfectly!
        : BWindow(BRect(0, 0, 1, 1), "Server Properties", B_MODAL_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS) {

        
        fParentWindow = parent;
        fItem = item;
        fServerIdx = serverIdx;
        fIsCustom = isCustom; // NEW: Track vector classification type inside instance memory

        AddToSubset(parent);
        CenterIn(parent->Frame());

        // Dynamic config lookup helper prevents code duplication
        ServerConfig& srv = GetActiveConfig();

        fNickInput = new BTextControl("nick", "Nickname:", srv.nick.c_str(), nullptr);
        fPassInput = new BTextControl("pass", "Password:", srv.pass.c_str(), nullptr);
        fPassInput->TextView()->HideTyping(true); 

        fServerListFontMenu = CreateFontMenu("Server List Font:", cfg.serverListFontSize);
        fChatLogFontMenu    = CreateFontMenu("Chat Log Font:", cfg.chatLogFontSize);
        fUserListFontMenu   = CreateFontMenu("User List Font:", cfg.userListFontSize);

        fAutoConnectCheck = new BCheckBox("autoconnect", "Automatically connect at startup", nullptr);
        fAutoConnectCheck->SetValue(srv.autoConnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fAutoReconnectCheck = new BCheckBox("autoreconnect", "Automatically reconnect on drop", nullptr);
        fAutoReconnectCheck->SetValue(srv.autoReconnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fDebugEnableCheck = new BCheckBox("debug", "Enable low-level socket engine logs", nullptr);
        fDebugEnableCheck->SetValue(cfg.debugEnable ? B_CONTROL_ON : B_CONTROL_OFF);

        fHideStatusCheck = new BCheckBox("hidestatus", "Hide channel status messages (Joins/Parts/Quits)", nullptr);
        fHideStatusCheck->SetValue(srv.hideStatusMessages ? B_CONTROL_ON : B_CONTROL_OFF);

        BButton* cancelBtn = new BButton("cancel", "Cancel", new BMessage('cfcn'));
        BButton* saveBtn = new BButton("save", "Save", new BMessage('cfsv'));
        saveBtn->MakeDefault(true);
        
		fQuitInput = new BTextControl("quitmsg", "QUIT Message:", cfg.quitMessage.c_str(), nullptr);
		
		BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
    		.SetInsets(15)
    		.AddGrid(5.0f, 5.0f)
        	.Add(fNickInput->CreateLabelLayoutItem(), 0, 0)
        	.Add(fNickInput->CreateTextViewLayoutItem(), 1, 0)
        	.Add(fPassInput->CreateLabelLayoutItem(), 0, 1)
        	.Add(fPassInput->CreateTextViewLayoutItem(), 1, 1)
        
        	// Renders the configuration input entry row neatly aligned
        	.Add(fQuitInput->CreateLabelLayoutItem(), 0, 2)
        	.Add(fQuitInput->CreateTextViewLayoutItem(), 1, 2)        

        	.Add(fServerListFontMenu->CreateLabelLayoutItem(), 0, 3)
        	.Add(fServerListFontMenu->CreateMenuBarLayoutItem(), 1, 3)
        	.Add(fChatLogFontMenu->CreateLabelLayoutItem(), 0, 4)
        	.Add(fChatLogFontMenu->CreateMenuBarLayoutItem(), 1, 4)
        	.Add(fUserListFontMenu->CreateLabelLayoutItem(), 0, 5)
        	.Add(fUserListFontMenu->CreateMenuBarLayoutItem(), 1, 5)
    	.End()
            .Add(fAutoConnectCheck)
            .Add(fAutoReconnectCheck)
            .Add(fHideStatusCheck)      
            .AddGlue()
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue()
                .Add(cancelBtn)
                .Add(saveBtn)
            .End();
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case 'cfcn':
                Quit();
                break;
                
            case 'cfsv': {
                // Dynamically route data saving to the correct active vector configuration structure 
                ServerConfig& srv = GetActiveConfig();
                srv.nick = fNickInput->Text();
                srv.pass = fPassInput->Text();
                srv.autoConnect = (fAutoConnectCheck->Value() == B_CONTROL_ON);
                srv.autoReconnect = (fAutoReconnectCheck->Value() == B_CONTROL_ON);
                cfg.debugEnable = (fDebugEnableCheck->Value() == B_CONTROL_ON);
				cfg.quitMessage = fQuitInput->Text();
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
    bool                fIsCustom; 
	BTextControl* 		fQuitInput;
    BTextControl*       fNickInput;
    BTextControl*       fPassInput;
    BCheckBox*          fAutoConnectCheck;
    BCheckBox*          fAutoReconnectCheck;
    BCheckBox*          fDebugEnableCheck;
    BCheckBox*          fHideStatusCheck;
    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;
	
    // NEW: Safe helper function to fetch correct active reference target safely
    ServerConfig& GetActiveConfig() {
        if (fIsCustom && fServerIdx < cfg.customServers.size()) {
            return cfg.customServers[fServerIdx];
        }
        // Fallback or standard hardcoded element reference loop block
        return cfg.servers[fServerIdx];
    }

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
        
        // NEW: Create an Add Server button pinned to the sidebar
        BButton* addServerButton = new BButton("add_srv_btn", "Add Server…", new BMessage('adcs')); // 'adcs' = Add Custom Server
        
        // 2. Load Config and Populate Servers Tree ONCE
        load_config(); 
        
        // fLiberaNode = nullptr;
        // fOftcNode = nullptr;

        // Loop A: Populate hardcoded default servers (Libera / OFTC)
        for (size_t i = 0; i < cfg.servers.size(); i++) {
            ServerTreeItem* serverNode = new ServerTreeItem(cfg.servers[i].name.c_str(), i, false, false);
            
            // Copy the loaded configuration values straight onto the UI tree items on boot!
            serverNode->SetHideStatus(cfg.servers[i].hideStatusMessages);
            
            fChannelTree->AddItem(serverNode);
            
            // Map structural lookups to our class reference tracking pointers
            if (i == 0) fLiberaNode = serverNode;
            if (i == 1) fOftcNode = serverNode;
        }

        // Loop B: Populate user-defined custom servers dynamically
        for (size_t i = 0; i < cfg.customServers.size(); i++) {
            ServerTreeItem* customNode = new ServerTreeItem(cfg.customServers[i].name.c_str(), i, false, true);
            
            // Copy configuration values onto custom items
            customNode->SetHideStatus(cfg.customServers[i].hideStatusMessages);
            
            // Add right into the exact same tree view!
            fChannelTree->AddItem(customNode);
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
            .AddGroup(B_HORIZONTAL, 5, 0.0) // Bottom bar remains fixed
                .Add(addServerButton, 0.0)   // Pins the "Add Server..." button on the far left
                // REMOVED GLUE: Let the input control weight do the expansion work natively!
                .Add(fInputControl, 1.0)     // Chat entry line automatically expands to fill the entire gap
                .Add(fConnectButton, 0.0)    // Join button remains pinned on the far right
            .End();




        // 6. Initialize State Properties
        fChannelTree->SetSelectionMessage(new BMessage('slch'));
        fActiveBufferItem = nullptr;        
        fLiberaThread = -1;
        fOftcThread = -1;
        fLiberaSocket = nullptr;
        fOftcSocket = nullptr;
        fCurrentServerNode = nullptr;
        
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
            if (srvNode != nullptr) {
                bool triggerConnect = false;
                size_t idx = srvNode->GetIndex();

                // Check the configuration storage location based on the server node classification type
                if (srvNode->IsCustom()) {
                    if (idx < cfg.customServers.size()) {
                        triggerConnect = cfg.customServers[idx].autoConnect;
                    }
                } else {
                    if (idx < cfg.servers.size()) {
                        triggerConnect = cfg.servers[idx].autoConnect;
                    }
                }

                if (triggerConnect) {
                    ConnectToServer(srvNode);
                }
            }
        }              
    }




    ~HIRCWindow() {
        // 1. Prepare the custom sign-off message payload
        BString quitPayload;
        quitPayload << "QUIT :" << cfg.quitMessage.c_str() << "\r\n";

        std::vector<std::pair<thread_id, BSecureSocket*>> shutdownSnapshot;
        
        Lock();
        for (auto const& [serverNode, socketPtr] : fServerSockets) {
            if (socketPtr != nullptr) {
                socketPtr->Write(quitPayload.String(), quitPayload.Length());
                thread_id tid = (fServerThreads.count(serverNode) > 0) ? fServerThreads[serverNode] : -1;
                shutdownSnapshot.push_back({tid, socketPtr});
            }
        }
        Unlock();

        // Give the networking hardware a brief moment to flush the QUIT strings out
        snooze(50000); // 50ms

        // 2. Disconnect all sockets to tell the threads to drop out of their loops
        for (auto const& [tid, socketPtr] : shutdownSnapshot) {
            if (socketPtr != nullptr) {
                socketPtr->Disconnect(); 
            }
        }

        // Give the background worker threads a quick window to wake up and close on their own
        snooze(50000); // 50ms

        // 3. FORCE KILL STUBBORN THREADS: Completely breaks any deadlocks
        for (auto const& [tid, socketPtr] : shutdownSnapshot) {
            if (tid >= 0) {
                // If the thread is still alive and didn't close gracefully, kill it instantly!
                // This stops it from trapping wait_for_thread in a deadlock.
                thread_info info;
                if (get_thread_info(tid, &info) == B_OK) {
                    kill_thread(tid); 
                }
            }
        }

        // 4. Clean up memory collections safely now that threads are guaranteed dead
        for (auto const& [key, listPtr] : fChannelUsers) {
            delete listPtr;
        }

        // 5. Force the application process to instantly terminate and leave the Deskbar
        be_app->PostMessage(B_QUIT_REQUESTED); 
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
            
            // DYNAMIC CONNECTION ROUTING: Use hardcoded tokens for defaults, generic for custom
            if (srvItem->IsCustom()) {
                BMessage* connectMsg = new BMessage(MSG_CONNECT_CUSTOM_SERVER);
                connectMsg->AddPointer("server_item", srvItem);
                menu->AddItem(new BMenuItem("Connect", connectMsg));
            } else {
                uint32 commandMsg = (srvItem == fLiberaNode) ? MSG_CONNECT_LIBERA : MSG_CONNECT_OFTC;
                menu->AddItem(new BMenuItem("Connect", new BMessage(commandMsg)));
            }
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

            // DYNAMIC DELETION: Allow custom servers to be completely purged from JSON configs
            if (srvItem->IsCustom()) {
                menu->AddSeparatorItem();
                BMessage* deleteMsg = new BMessage('dlcs'); // Define MSG_DELETE_CUSTOM_SERVER as 'dlcs'
                deleteMsg->AddPointer("server_item", srvItem);
                
                BMenuItem* deleteItem = new BMenuItem("Delete Custom Server", deleteMsg);
                // Give it a warning color state if supported, or leave plain
                menu->AddItem(deleteItem);
            }
            
            menu->SetTargetForItems(this);
            menu->Go(screenPoint, true, true, true);
        }
    }







    void ConnectToServer(ServerTreeItem* targetNode) {
        if (targetNode == nullptr) return;

        // 1. Check if this specific server is already connecting/connected
        if (fServerThreads.count(targetNode) > 0 && fServerThreads[targetNode] >= 0) {
            return; 
        }
        
        // 2. Build and insert the Server raw log node item instantly under the target node header
        BStringItem* serverLogItem = new BStringItem("[Server]");
        fChannelTree->AddUnder(serverLogItem, targetNode);
        fChannelTree->Expand(targetNode);
        
        // If nothing is selected yet, make this new server log view active
        if (fActiveBufferItem == nullptr) fActiveBufferItem = serverLogItem;

        // 3. Dynamically read the name from the item text for the log buffer echo
        BString logMessage;
        logMessage.SetToFormat("Connecting to %s...\n", targetNode->Text());
        fTextBuffers[serverLogItem] << logMessage;
        fChatLog->SetText(fTextBuffers[serverLogItem].String());

        // 4. Generate a unique worker name thread label string dynamically
        BString threadName;
        threadName.SetToFormat("%sWorker", targetNode->Text());
        // Clean out spaces from the string so thread creation stays tidy
        threadName.ReplaceAll(" ", ""); 

        // 5. Spawn the thread and associate it natively inside our index tracker map
        thread_id newThread = spawn_thread(NetworkLoop, threadName.String(), B_NORMAL_PRIORITY, targetNode);
        if (newThread >= 0) {
            fServerThreads[targetNode] = newThread;
            
            // Map legacy variables to prevent breakages elsewhere in app
            if (targetNode == fLiberaNode) fLiberaThread = newThread;
            if (targetNode == fOftcNode)   fOftcThread = newThread;

            resume_thread(newThread);
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
        
        // 2. Prepare our custom font variations based on config size
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


void ParseAndDisplayIRC(BString line, ServerTreeItem* contextServer) {   	    	
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

        // UPDATED: Use the passed context connection. Fallback to Libera only if null.
        if (contextServer == nullptr) {
            contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
        }

        // Catch Successful Join actions (Handles Us AND Other Users)
        if (command == "JOIN") {
            BString channelJoined = trailing.Length() > 0 ? trailing : line;
            channelJoined.ReplaceAll(" ", "");
            
            BString userWhoJoined = prefix;
            int32 exclamIdx = userWhoJoined.FindFirst("!");
            if (exclamIdx != B_ERROR) userWhoJoined.Truncate(exclamIdx);

            // UPDATED: Target the specific server connection context pointer explicitly
            ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelJoined);
            if (!chanNode) {
                // FIXED: Pass contextServer's custom status flag down into the child channel item node
                ChannelTreeItem* newChanNode = new ChannelTreeItem(channelJoined.String(), 
                                                                   contextServer->GetIndex(), 
                                                                   contextServer->IsCustom());

                fChannelTree->AddUnder(newChanNode, contextServer); 
                fChannelTree->Expand(contextServer);
                
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
                        
                        // UPDATED: Read status visibility suppression from the explicitly scoped node
                        bool hideStatusOnThisServer = contextServer->IsHideStatus();
                        
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

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // UPDATED: Only feed the row item over if the active panel matches this specific network context stream
            if (fActiveListWindow != nullptr) {
                // Determine the socket the open window is listening to
                BSecureSocket* windowTargetSocket = fActiveListWindow->GetTargetSocket(); 
                BSecureSocket* activeNetworkSocket = fServerSockets[contextServer];

                // Alternative: If window tracks the server pointer instead of raw sockets:
                // if (fActiveListWindow->GetServerContext() == contextServer)

                if (windowTargetSocket == activeNetworkSocket) {
                    BMessage* rowPackage = new BMessage(MSG_ADD_LIST_ROW);
                    rowPackage->AddString("channel", channelName.String());
                    rowPackage->AddString("users", userCount.String());
                    rowPackage->AddString("topic", trailing.String());
                    fActiveListWindow->PostMessage(rowPackage);
                }
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

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // UPDATED: Look strictly through the channels assigned to the contextServer that received the data
            int32 underCount = fChannelTree->CountItemsUnder(contextServer, true);
            for (int32 c = 0; c < underCount; c++) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(contextServer, true, c));
                if (!chanNode) continue;
                
                // If it's a room-specific PART, skip loops on non-matching channel strings
                if (command == "PART" && BString(chanNode->Text()) != targetChannel) continue;

                if (fChannelUsers[chanNode] != nullptr) {
                    for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                        BString itemTxt = fChannelUsers[chanNode]->ItemAt(i)->Text();
                        
                        // Explicitly clean up the prefix *before* checking against the user string
                        if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                        
                        if (itemTxt == userWhoLeft) {
                            // FIXED: In Haiku, removing an item from a list doesn't delete it.
                            // Pass the unlinked pointer to delete to completely free its heap memory!
                            delete fChannelUsers[chanNode]->RemoveItemAt(i);
                            
                            // UPDATED: Query the specific, thread-safe server context node for configuration properties
                            bool hideStatusOnThisServer = contextServer->IsHideStatus();
                            
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

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // UPDATED: Query the specific network context mapping explicitly
            BStringItem* chanNode = FindChannelNode(contextServer, targetChannel);
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

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // UPDATED: Use contextServer directly to resolve multi-server namespace clashes safely
            BStringItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode && fChannelUsers[chanNode] != nullptr) {
                int32 spaceIdx;
                while ((spaceIdx = trailing.FindFirst(" ")) != B_ERROR) {
                    BString singleUser;
                    trailing.CopyInto(singleUser, 0, spaceIdx);
                    trailing.Remove(0, spaceIdx + 1);
                    
                    if (singleUser.Length() > 0) {
                        fChannelUsers[chanNode]->AddItem(new BStringItem(singleUser.String()));
                    }
                }
                if (trailing.Length() > 0) {
                    fChannelUsers[chanNode]->AddItem(new BStringItem(trailing.String()));
                }

                // Instead of manually duplicating raw pointers directly into fUserList,
                // trigger pre-existing method to let it apply font scales and avoid memory leaks.
                if (fActiveBufferItem == chanNode) {
                    RefreshUserListUI();
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

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) contextServer = fLiberaNode;

            // Check nickname against the specific server's configuration
            if (oldNick.ICompare(contextServer->GetNick()) == 0) {
                // Update the configuration vector directly so GetNick() returns the new nickname
                size_t serverIdx = contextServer->GetIndex();
                if (contextServer->IsCustom() && serverIdx < cfg.customServers.size()) {
                    cfg.customServers[serverIdx].nick = newNick.String();
                } else if (!contextServer->IsCustom() && serverIdx < cfg.servers.size()) {
                    cfg.servers[serverIdx].nick = newNick.String();
                }
                
                BString itemNotice;
                itemNotice << "--- Your nickname on this server is now " << newNick << "\n";
                LogToItemBuffer(fActiveBufferItem, itemNotice);
                
                // Keep fMyNick sync'd for backwards compatibility if needed elsewhere
                fMyNick = newNick;
            }

            // UPDATED: Loop through items strictly belonging to the contextServer that received the data
            int32 underCount = fChannelTree->CountItemsUnder(contextServer, true);
            for (int32 c = 0; c < underCount; c++) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(contextServer, true, c));
                if (chanNode && fChannelUsers[chanNode] != nullptr) {
                    
                    // We must delete the unlinked item manually in this loop to prevent memory leaks!
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
                            
                            // FIXED: In Haiku, removing an item from a list doesn't delete it. 
                            // Delete the old row item explicitly to prevent an ongoing memory leak!
                            delete fChannelUsers[chanNode]->RemoveItemAt(i);
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
            
            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // 1. UPDATED: Establish the destination target node item reference using contextServer
            BStringItem* targetNode = nullptr;
            if (targetRoom.StartsWith("#")) {
                targetNode = FindChannelNode(contextServer, targetRoom);
            }
            if (targetNode == nullptr) {
                targetNode = FindServerLogNode(contextServer);
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
                
                // Pull the authentic communication pipe using lookup map safely
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                
                if (activeSocket != nullptr && contextServer != nullptr) {
                    BString savedPassword = contextServer->GetPass();
                    
                    if (savedPassword.Length() > 0) {
                        BString autoIdentify;
                        autoIdentify << "PRIVMSG NickServ :IDENTIFY " << savedPassword << "\r\n";
                        activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                        
                        BString logNotice = "--- [Auto-Services] Identification credentials automatically sent to NickServ.\n";
                        LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
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
                    LogToItemBuffer(FindServerLogNode(contextServer), formattedMsg);
                }
            } else {
                LogToItemBuffer(FindServerLogNode(contextServer), formattedMsg);
            }
        }

        
        else if (trailing.Length() > 0) {
            // AUTOMATION HOOK FALLBACK: Intercept raw notices before registration fully establishes
            if (prefix.IFindFirst("NickServ") != B_ERROR && trailing.IFindFirst("identify") != B_ERROR) {
                
                // UPDATED: Dynamically fetch active socket mapping stream
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }

                if (activeSocket != nullptr && contextServer != nullptr && contextServer->GetPass().Length() > 0) {
                    BString autoIdentify;
                    autoIdentify << "PRIVMSG NickServ :IDENTIFY " << contextServer->GetPass() << "\r\n";
                    activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                    
                    BString logNotice = "--- [Auto-Services] Pre-registration identification sent to NickServ.\n";
                    LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
                }
            }

            // General Server Protocol output (MOTD, Notices, Connection headers) -> routes to "[Server]" tree node
            // UPDATED: Route using explicit context pointer safely
            BStringItem* svrLogNode = FindServerLogNode(contextServer);
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

        // UPDATED: Dynamically track socket connection instances using our tracker map
        window->Lock(); // Ensure thread-safe map insertion
        window->fServerSockets[targetNode] = localSocket;
        
        // Map legacy pointers to prevent breakages elsewhere 
        if (targetNode == window->fLiberaNode) window->fLiberaSocket = localSocket;
        if (targetNode == window->fOftcNode)   window->fOftcSocket = localSocket;
        window->Unlock();

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

        // 5. Thread and Socket Cleanup State Reset
        bool triggerReconnect = targetNode->IsAutoReconnect();
        
        // Before accessing the window maps, ensure the window is still fully valid and active
        if (be_app->CountWindows() > 0 && be_app->WindowAt(0) == window) {
            if (window->Lock()) { // Double check thread safety lock boundaries
                window->fServerSockets.erase(targetNode);
                window->fServerThreads.erase(targetNode);
                
                if (targetNode == window->fLiberaNode) {
                    window->fLiberaThread = -1;
                    window->fLiberaSocket = nullptr;
                } else if (targetNode == window->fOftcNode) {
                    window->fOftcThread = -1;
                    window->fOftcSocket = nullptr;
                }
                window->Unlock();
                
                // Only post a reconnection message if the parent workspace isn't shutting down
                if (triggerReconnect) {
                    BMessage* reconnectMessage = new BMessage(MSG_RECONNECT_SERVER);
                    reconnectMessage->AddPointer("server_item", targetNode);
                    window->PostMessage(reconnectMessage);
                }
            }
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
                            
                            // 1. Auto-Join toggle option
                            BString label = chanItem->IsAutoJoin() ? "Disable Auto-Join" : "Enable Auto-Join";
                            BMessage* toggleMsg = new BMessage(MSG_TOGGLE_AUTOJOIN);
                            toggleMsg->AddPointer("chan_item", chanItem); 
                            menu->AddItem(new BMenuItem(label.String(), toggleMsg));
                            
                            // 2. Append a separator and new Remove Channel item
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

        case 'dlcs': { // Delete Custom Server Message
            ServerTreeItem* srvItem;
            if (message->FindPointer("server_item", (void**)&srvItem) == B_OK && srvItem != nullptr) {
                size_t targetIndex = srvItem->GetIndex();
                
                if (srvItem->IsCustom() && targetIndex < cfg.customServers.size()) {
                    // 1. Erase item block directly out of global structural configurations vector array
                    cfg.customServers.erase(cfg.customServers.begin() + targetIndex);
                    
                    // 2. Remove the matching object memory block container node directly out of the visual sidebar tree
                    fChannelTree->RemoveItem(srvItem);
                    delete srvItem;

                    // 3. IMPORTANT: Update the internal indexes of remaining Custom Server items in the list tree
                    for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
                        ServerTreeItem* current = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(i));
                        if (current != nullptr && current->IsCustom()) {
                            // If it sat further down the list past the item we deleted, step its internal index back by 1
                            size_t curIdx = current->GetIndex();
                            if (curIdx > targetIndex) {
                                // Re-instantiate or mutate item's internal property. 
                                // Since fields are private, if you don't have a SetIndex helper, you can do:
                                // current->fConfigIndex--; (if HIRCWindow is a friend class)
                                // Better: add void SetIndex(size_t idx) { fConfigIndex = idx; } to ServerTreeItem
                                current->SetIndex(curIdx - 1); 
                            }
                        }
                    }

                    // 4. Update config text file immediately
                    save_config();
                    fChannelTree->Invalidate();
                }
            }
            break;
        }

        case 'adcs': {            
            AddServerWindow* dlg = new AddServerWindow(this);
            dlg->Show(); 
            break;
        }

        case MSG_ADD_CUSTOM_SERVER_SUBMIT: {
            BString name, host, nick, pass;
            int32 port;

            if (message->FindString("name", &name) == B_OK &&
                message->FindString("host", &host) == B_OK &&
                message->FindInt32("port", &port) == B_OK &&
                message->FindString("nick", &nick) == B_OK) {
                
                message->FindString("pass", &pass); // Can remain blank safely

                // 1. Build the data structure block matching settings scheme
                ServerConfig customSrv;
                customSrv.name = name.String();
                customSrv.host = host.String();
                customSrv.port = static_cast<uint16>(port);
                customSrv.nick = nick.String();
                customSrv.pass = pass.String();
                customSrv.autoConnect = false;
                customSrv.autoReconnect = false;
                customSrv.hideStatusMessages = false;

                // 2. Append directly to the config struct vector array and save file changes
                cfg.customServers.push_back(customSrv);
                save_config();

                // 3. Update the UI tree instantly on screen without restarting!
                size_t newIndex = cfg.customServers.size() - 1;
                ServerTreeItem* customNode = new ServerTreeItem(customSrv.name.c_str(), newIndex, false, true);
                
                fChannelTree->AddItem(customNode);
                fChannelTree->Invalidate(); // Force Haiku redraw interface block
            }
            break;
        }



        	
    case 'rmch': { // Remove Channel action handler
        ChannelTreeItem* chanItem = nullptr;
        
        if (message->FindPointer("channel_item", (void**)&chanItem) == B_OK && chanItem != nullptr) {
            
            // 1. Send network message to leave communication space gracefully if socket exists
            ServerTreeItem* parentServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(chanItem));
            if (parentServer != nullptr) {
                
                // UPDATED: Dynamically pull the correct network connection out of our lookup map
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(parentServer) > 0) {
                    activeSocket = fServerSockets[parentServer];
                } else {
                    activeSocket = (parentServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }

                if (activeSocket != nullptr) {
                    BString partCmd;
                    partCmd << "PART " << chanItem->Text() << " :Channel removed by user\r\n";
                    activeSocket->Write(partCmd.String(), partCmd.Length());
                }
                
                // 2. Remove the channel name from the correct configuration's autojoin vector array
                size_t serverIdx = parentServer->GetIndex();
                BString targetChanName(chanItem->Text());
                
                // Remove dynamic tracking suffix text tags if present (like " [A]") before matching
                int32 tagPos = targetChanName.FindFirst(" [");
                if (tagPos != B_ERROR) targetChanName.Truncate(tagPos);

                // UPDATED: Point to either customServers or standard servers vector securely using the item flag
                if (parentServer->IsCustom()) {
                    if (serverIdx < cfg.customServers.size()) {
                        auto& autojoinVec = cfg.customServers[serverIdx].autojoin;
                        for (auto it = autojoinVec.begin(); it != autojoinVec.end(); ) {
                            if (targetChanName.ICompare(it->c_str()) == 0) {
                                it = autojoinVec.erase(it);
                            } else {
                                ++it;
                            }
                        }
                    }
                } else {
                    if (serverIdx < cfg.servers.size()) {
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
            }

            // 3. Wipe current chat logs from active memory views if we delete the room we are looking at
            if (fActiveBufferItem == chanItem) {
                fActiveBufferItem = nullptr;
                fChatLog->SetText("");
                
                // FIXED: Manually wipe and completely delete layout rows to prevent memory leaks
                while (fUserList->CountItems() > 0) {
                    delete fUserList->RemoveItem((int32)0);
                }
                
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
        topicFont.SetSize(cfg.chatLogFontSize); // Make chat log text size
        fTopicView->SetFont(&topicFont);
        fTopicView->TextView()->SetFontAndColor(&topicFont, B_FONT_SIZE); // Updates the editable region
        fTopicView->InvalidateLayout();
        fTopicView->Invalidate();

        // 3. Clear and rebuild the BTextView history buffer (Center panel)
        BFont chatFont;
        fChatLog->GetFont(&chatFont);
        chatFont.SetSize(cfg.chatLogFontSize);
        fChatLog->SetFont(&chatFont);

        // Tell the text view to smoothly scale all characters to the new size
        fChatLog->SetFontAndColor(&chatFont, B_FONT_SIZE); 
        fChatLog->Invalidate();


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
            
            size_t matchingIndex = item->GetIndex(); // Extract the true index directly from the item!
            bool isCustom = item->IsCustom();        // Extract whether it is custom or default
            
            // Pass the custom flag as a new 4th argument to the dialog window constructor
            ServerConfigWindow* configWin = new ServerConfigWindow(this, item, matchingIndex, isCustom);
            configWin->Show();
        }
        break;
    }


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
                          << "Version 0.0.4\n"
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
            		// 1. Flip the state inside the visible UI tree item element
            		bool newStatus = !item->IsHideStatus();
            		item->SetHideStatus(newStatus);
            
            		size_t targetIndex = item->GetIndex();

            		// 2. UPDATED: Directly target the correct array vector index using properties
            		if (item->IsCustom()) {
                        if (targetIndex < cfg.customServers.size()) {
                            cfg.customServers[targetIndex].hideStatusMessages = newStatus;
                        }
            		} else {
                        if (targetIndex < cfg.servers.size()) {
                            cfg.servers[targetIndex].hideStatusMessages = newStatus;
                        }
            		}
            		
            		// 3. Commit changes to hircConfig.txt JSON file instantly
            		save_config(); 
        		}
        		break;
    		}


        	
        	
            case MSG_CONTEXT_CHAN_LIST: {
                void* ptr;
                // FIXED: FindPointer requires a pointer-to-a-pointer (void**)&ptr cast to work correctly
                if (message->FindPointer("server_item", (void**)&ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        
                        // UPDATED: Dynamically fetch the accurate active network connection from our lookup map
                        BSecureSocket* activeSocket = nullptr;
                        if (fServerSockets.count(serverItem) > 0) {
                            activeSocket = fServerSockets[serverItem];
                        } else {
                            // Backwards-compatibility fallback for default servers on startup
                            activeSocket = (serverItem == fOftcNode) ? fOftcSocket : fLiberaSocket;
                        }
                        
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
                            BString warning;
                            warning.SetToFormat("System Error: You must connect to '%s' first before requesting a channel list.\n", serverItem->Text());
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
                // FIXED: Cast to (void**)&ptr to comply with the Haiku FindPointer API signature
                if (message->FindPointer("server_item", (void**)&ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        // Invert state value
                        bool newState = !serverItem->IsAutoConnect();
                        serverItem->SetAutoConnect(newState);
                        
                        // UPDATED: Route the configuration assignment to the correct target profile vector
                        if (serverItem->IsCustom()) {
                            if (idx < cfg.customServers.size()) {
                                cfg.customServers[idx].autoConnect = newState;
                            }
                        } else {
                            if (idx < cfg.servers.size()) {
                                cfg.servers[idx].autoConnect = newState;
                            }
                        }
                        
                        // Flush to settings file and force immediate sidebar graphic redraw loop pass
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
                // FIXED: Cast to (void**)&ptr to comply with the Haiku FindPointer API signature
                if (message->FindPointer("chan_item", (void**)&ptr) == B_OK) {
                    ChannelTreeItem* chanItem = static_cast<ChannelTreeItem*>(ptr);
                    if (chanItem != nullptr) {
                        size_t srvIdx = chanItem->GetServerIndex();
                        std::string targetChan = chanItem->Text();
                        
                        // UPDATED: Route data manipulation to customServers or servers using the item flag
                        if (chanItem->IsCustom()) {
                            if (srvIdx < cfg.customServers.size()) {
                                auto& vec = cfg.customServers[srvIdx].autojoin;
                                if (chanItem->IsAutoJoin()) {
                                    // Deactivate: Remove the string leaf value from our configuration vector
                                    vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                                    chanItem->SetAutoJoin(false);
                                } else {
                                    // Activate: Append string into configuration profile
                                    vec.push_back(targetChan);
                                    chanItem->SetAutoJoin(true);
                                }
                            }
                        } else {
                            if (srvIdx < cfg.servers.size()) {
                                auto& vec = cfg.servers[srvIdx].autojoin;
                                if (chanItem->IsAutoJoin()) {
                                    // Deactivate: Remove the string leaf value from our configuration vector
                                    vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                                    chanItem->SetAutoJoin(false);
                                } else {
                                    // Activate: Append string into configuration profile
                                    vec.push_back(targetChan);
                                    chanItem->SetAutoJoin(true);
                                }
                            }
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
                         	while (fUserList->CountItems() > 0) {
    						delete fUserList->RemoveItem((int32)0);
							}

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
                                            
                                            // Force individual user rows to calculate heights at custom scale
                                            newUserItem->Update(fUserList, &userListFont);
                                        }
                                    }
                                    fUserList->InvalidateLayout();
                                    fUserList->Invalidate();
                                }
                            }
                        }

						// 3. Re-populate visible chat log panel cleanly using configuration dimensions
							if (fTextBuffers.find(fActiveBufferItem) != fTextBuffers.end()) {
    
    							BFont customChatFont;
    							fChatLog->GetFont(&customChatFont);
    							customChatFont.SetSize(cfg.chatLogFontSize);
    							rgb_color currentThemeTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);
    
    							fChatLog->SetText("");
    							fChatLog->SetFontAndColor(&customChatFont, B_FONT_ALL, &currentThemeTextColor);
    
    							// Populating raw text directly into the view avoids formatting glitch complexities
    							fChatLog->Insert(fTextBuffers[fActiveBufferItem].String());
							} else {
    							fChatLog->SetText("");							}

                        
                        // Force the scrollbar viewport directly back down to the bottom
                        int32 newLength = fChatLog->TextLength();
                        fChatLog->Select(newLength, newLength);
                        fChatLog->ScrollToSelection();

                    }
                }
                break;
            }


            case MSG_CONNECT_CUSTOM_SERVER: { 
                ServerTreeItem* customNode = nullptr;
                if (message->FindPointer("server_item", (void**)&customNode) == B_OK && customNode != nullptr) {
                    ConnectToServer(customNode);
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

                    // 1. Fallback safety constraint mapping
                    if (contextServer == nullptr) {
                        contextServer = fLiberaNode;
                    }

                    // 2. UPDATED: Dynamic lookup matching from our new socket array map
                    BSecureSocket* activeSocket = nullptr;
                    if (fServerSockets.count(contextServer) > 0) {
                        activeSocket = fServerSockets[contextServer];
                    } else {
                        // Backwards-compatibility fallback if thread map mapping hasn't filled yet
                        activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                    }


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

                                    // 3. Render action locally
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
                                    
                                    // Echo the outgoing whisper locally into active window
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
                void* nodePtr = nullptr;
                if (message->FindString("text", &rawLine) == B_OK) {
                    if (message->FindPointer("server_node", (void**)&nodePtr) == B_OK && nodePtr != nullptr) {
                        fCurrentServerNode = static_cast<ServerTreeItem*>(nodePtr);
                    }
                    
                    // FIXED: Forward the true context pointer explicitly down to the processing logic
                    ParseAndDisplayIRC(rawLine, fCurrentServerNode);
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
    std::map<ServerTreeItem*, thread_id> fServerThreads;
    std::map<ServerTreeItem*, BSecureSocket*> fServerSockets;

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
