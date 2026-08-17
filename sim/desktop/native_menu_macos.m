// Native macOS menus for the SDL port. Menu key equivalents double as the
// keyboard shortcuts (NSApp dispatches them from the same event pump SDL
// runs), and every action fires on the main thread INSIDE the LVGL loop's
// locked region - so calling real ui_* functions directly here is safe, and
// taking display_lock() again would deadlock. Compiled with ARC (see
// CMakeLists.txt); non-Apple builds get native_menu_stub.c instead.
//
//   <App>  > Settings...        Cmd-,   audio input + transceiver pickers
//   File   > Open WAV...        Cmd-O   inject a WAV as the audio source
//   File   > CAT Log            Cmd-L   live in/out CAT traffic (mock or real)
//   View   > Settings Drawer    Cmd-D   toggle the right-edge drawer
//   View   > Panadapter / FT8   Cmd-T   toggle the base screen
//   View   > Memory Channels    Cmd-M   open the memory modal
#import <Cocoa/Cocoa.h>
#include "sim_devices.h"
#include "device_list.h"
#include "mock_rig.h"
#include "cat_log.h"
#include "ui.h"
#include "memory_modal.h"
#include "ui_mode.h"

static NSWindow *s_log_window;
static NSTextView *s_log_view;

static void append_log_line(const char *line)
{
    NSString *str = [[NSString alloc] initWithUTF8String:line];
    if (!str) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!s_log_view) return;
        NSDictionary *attrs = @{
            NSFontAttributeName : [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular],
            NSForegroundColorAttributeName : [NSColor textColor],
        };
        NSAttributedString *as =
            [[NSAttributedString alloc] initWithString:[str stringByAppendingString:@"\n"]
                                            attributes:attrs];
        [[s_log_view textStorage] appendAttributedString:as];
        [s_log_view scrollRangeToVisible:NSMakeRange(s_log_view.string.length, 0)];
    });
}

static NSTextField *mk_label(NSString *text, NSRect frame)
{
    NSTextField *l = [NSTextField labelWithString:text];
    l.frame = frame;
    l.alignment = NSTextAlignmentRight;
    return l;
}

@interface SimMenuActions : NSObject
@end

@implementation SimMenuActions

- (void)openWav:(id)sender
{
    NSOpenPanel *p = [NSOpenPanel openPanel];
    p.canChooseDirectories = NO;
    p.allowsMultipleSelection = NO;
    if ([p runModal] == NSModalResponseOK && p.URL) {
        if (!sim_audio_select_wav(p.URL.fileSystemRepresentation)) {
            NSAlert *a = [NSAlert new];
            a.messageText = @"Could not load WAV";
            a.informativeText = @"Only 16-bit PCM mono/stereo WAV files are supported.";
            [a runModal];
        }
    }
}

- (void)openCatLog:(id)sender
{
    if (!s_log_window) {
        NSRect r = NSMakeRect(0, 0, 640, 420);
        s_log_window = [[NSWindow alloc]
            initWithContentRect:r
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        s_log_window.title = @"CAT Log";
        s_log_window.releasedWhenClosed = NO; // closing hides; reopening keeps history

        NSScrollView *sv = [[NSScrollView alloc] initWithFrame:r];
        sv.hasVerticalScroller = YES;
        sv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        NSTextView *tv = [[NSTextView alloc] initWithFrame:r];
        tv.editable = NO;
        tv.minSize = NSMakeSize(0, 0);
        tv.maxSize = NSMakeSize(FLT_MAX, FLT_MAX);
        tv.verticallyResizable = YES;
        tv.horizontallyResizable = NO;
        tv.autoresizingMask = NSViewWidthSizable;
        tv.textContainer.containerSize = NSMakeSize(r.size.width, FLT_MAX);
        tv.textContainer.widthTracksTextView = YES;

        sv.documentView = tv;
        s_log_window.contentView = sv;
        [s_log_window center];
        s_log_view = tv;
        cat_log_set_listener(append_log_line); // replays buffered history first
    }
    [s_log_window makeKeyAndOrderFront:nil];
}

- (void)openSettings:(id)sender
{
    device_list_t audio_devs, serial_devs;
    device_list_audio_inputs(&audio_devs);
    device_list_serial_ports(&serial_devs);

    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 430, 76)];

    [view addSubview:mk_label(@"Audio input:", NSMakeRect(0, 46, 120, 20))];
    NSPopUpButton *audio_pop =
        [[NSPopUpButton alloc] initWithFrame:NSMakeRect(128, 42, 296, 26) pullsDown:NO];
    // Use the popup's NSMenu directly - addItemWithTitle: on the button
    // silently drops duplicate titles, and device names can repeat.
    [audio_pop.menu addItemWithTitle:@"None (silence)" action:nil keyEquivalent:@""];
    int wav_item = -1;
    if (sim_audio_get_source_kind() == 1) {
        NSString *base = [[NSString stringWithUTF8String:sim_audio_get_source_name()]
                             lastPathComponent];
        [audio_pop.menu addItemWithTitle:[NSString stringWithFormat:@"WAV: %@ (active)", base]
                                  action:nil
                           keyEquivalent:@""];
        wav_item = 1;
    }
    int audio_dev_base = (int)audio_pop.menu.numberOfItems;
    for (int i = 0; i < audio_devs.count; i++)
        [audio_pop.menu addItemWithTitle:[NSString stringWithUTF8String:audio_devs.names[i]]
                                  action:nil
                           keyEquivalent:@""];
    if (wav_item >= 0) [audio_pop selectItemAtIndex:wav_item];
    else if (sim_audio_get_source_kind() == 2) {
        for (int i = 0; i < audio_devs.count; i++)
            if (strcmp(audio_devs.names[i], sim_audio_get_source_name()) == 0)
                [audio_pop selectItemAtIndex:audio_dev_base + i];
    } else {
        [audio_pop selectItemAtIndex:0];
    }
    [view addSubview:audio_pop];

    [view addSubview:mk_label(@"Transceiver:", NSMakeRect(0, 10, 120, 20))];
    NSPopUpButton *rig_pop =
        [[NSPopUpButton alloc] initWithFrame:NSMakeRect(128, 6, 296, 26) pullsDown:NO];
    int n_mock = mock_rig_count();
    for (int i = 0; i < n_mock; i++)
        [rig_pop.menu addItemWithTitle:[NSString stringWithUTF8String:mock_rig_name(i)]
                                action:nil
                         keyEquivalent:@""];
    for (int i = 0; i < serial_devs.count; i++)
        [rig_pop.menu addItemWithTitle:[NSString stringWithUTF8String:serial_devs.names[i]]
                                action:nil
                         keyEquivalent:@""];
    const char *cur_port = sim_cat_get_port();
    if (cur_port[0]) {
        for (int i = 0; i < serial_devs.count; i++)
            if (strcmp(serial_devs.names[i], cur_port) == 0)
                [rig_pop selectItemAtIndex:n_mock + i];
    } else {
        [rig_pop selectItemAtIndex:mock_rig_selected()];
    }
    [view addSubview:rig_pop];

    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Signal sources";
    alert.informativeText = @"A QMX plugged into this machine appears as both an "
                            @"audio input and a serial port.";
    alert.accessoryView = view;
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) return;

    // Apply audio choice.
    NSInteger ai = audio_pop.indexOfSelectedItem;
    if (ai == 0) {
        if (sim_audio_get_source_kind() != 0) sim_audio_select_none();
    } else if (ai == wav_item) {
        // keep the running WAV
    } else {
        const char *name = audio_pop.selectedItem.title.UTF8String;
        if (sim_audio_get_source_kind() != 2 || strcmp(name, sim_audio_get_source_name()) != 0)
            sim_audio_select_capture(name);
    }

    // Apply transceiver choice.
    NSInteger ri = rig_pop.indexOfSelectedItem;
    if (ri < n_mock) {
        mock_rig_select((int)ri);
        if (cur_port[0]) sim_cat_select_none();
    } else {
        const char *path = rig_pop.selectedItem.title.UTF8String;
        if (strcmp(path, cur_port) != 0) {
            if (!sim_cat_select_port(path)) {
                NSAlert *a = [NSAlert new];
                a.messageText = @"Could not open serial port";
                a.informativeText = [NSString stringWithUTF8String:path];
                [a runModal];
            }
        }
    }
}

// View menu - runs on the LVGL thread inside its locked region (see the
// file comment), so these call the real UI directly, exactly like a touch
// callback would.
- (void)toggleDrawer:(id)sender { ui_set_drawer_open(!ui_drawer_is_open()); }
- (void)toggleScreen:(id)sender { ui_request_base_mode(ui_mode_get() != UI_MODE_FT8); }
- (void)openMemories:(id)sender { memory_modal_show(); }

@end

static SimMenuActions *s_actions;

void native_menu_install(void)
{
    @autoreleasepool {
        NSMenu *main_menu = [NSApp mainMenu];
        if (!main_menu) return; // not on macOS's normal SDL path - no menus then
        s_actions = [SimMenuActions new];

        // Settings... into the application menu, Cmd-, like every Mac app.
        NSMenu *app_menu = [[main_menu itemAtIndex:0] submenu];
        NSMenuItem *settings = [[NSMenuItem alloc] initWithTitle:@"Settings…"
                                                          action:@selector(openSettings:)
                                                   keyEquivalent:@","];
        settings.target = s_actions;
        [app_menu insertItem:[NSMenuItem separatorItem] atIndex:1];
        [app_menu insertItem:settings atIndex:2];

        NSMenu *file_menu = [[NSMenu alloc] initWithTitle:@"File"];
        NSMenuItem *open_wav = [[NSMenuItem alloc] initWithTitle:@"Open WAV…"
                                                          action:@selector(openWav:)
                                                   keyEquivalent:@"o"];
        open_wav.target = s_actions;
        [file_menu addItem:open_wav];
        NSMenuItem *cat_log = [[NSMenuItem alloc] initWithTitle:@"CAT Log"
                                                         action:@selector(openCatLog:)
                                                  keyEquivalent:@"l"];
        cat_log.target = s_actions;
        [file_menu addItem:cat_log];
        NSMenuItem *file_item = [[NSMenuItem alloc] initWithTitle:@"File"
                                                           action:nil
                                                    keyEquivalent:@""];
        file_item.submenu = file_menu;
        [main_menu insertItem:file_item atIndex:1];

        NSMenu *view_menu = [[NSMenu alloc] initWithTitle:@"View"];
        struct { NSString *title; SEL sel; NSString *key; } items[] = {
            { @"Settings Drawer", @selector(toggleDrawer:), @"d" },
            { @"Panadapter / FT8", @selector(toggleScreen:), @"t" },
            { @"Memory Channels", @selector(openMemories:), @"m" },
        };
        for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
            NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle:items[i].title
                                                        action:items[i].sel
                                                 keyEquivalent:items[i].key];
            mi.target = s_actions;
            [view_menu addItem:mi];
        }
        NSMenuItem *view_item = [[NSMenuItem alloc] initWithTitle:@"View"
                                                           action:nil
                                                    keyEquivalent:@""];
        view_item.submenu = view_menu;
        [main_menu insertItem:view_item atIndex:2];
    }
}
