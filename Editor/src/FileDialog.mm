#include "FileDialog.h"

#include <Nova/Platform/MacApp.h>

#import <AppKit/AppKit.h>

#include <vector>

namespace Nova::Editor {

namespace {

NSString* ToNSString(const std::string& s) {
    return [NSString stringWithUTF8String:s.c_str()];
}

std::optional<std::filesystem::path> PathFromNSURL(NSURL* url) {
    if (!url) return std::nullopt;
    const char* path = url.fileSystemRepresentation;
    if (!path) return std::nullopt;
    return std::filesystem::path(path);
}

NSArray<NSString*>* BuildAllowedTypes(const std::vector<FileFilter>& filters) {
    NSMutableArray<NSString*>* types = [NSMutableArray array];
    for (const FileFilter& filter : filters) {
        for (const std::string& ext : filter.Extensions) {
            [types addObject:ToNSString(ext)];
        }
    }
    if (types.count == 0) {
        [types addObject:@"json"];
    }
    return types;
}

} // namespace

std::optional<std::filesystem::path> ShowOpenFolderDialog(const char* title) {
    Nova::EnsureMacOSApplicationReady();
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = title ? ToNSString(title) : @"Open Folder";
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.allowsMultipleSelection = NO;
        panel.canCreateDirectories = YES;
        if ([panel runModal] != NSModalResponseOK) {
            return std::nullopt;
        }
        return PathFromNSURL(panel.URL);
    }
}

std::optional<std::filesystem::path> ShowOpenFileDialog(const char* title,
                                                        const std::vector<FileFilter>& filters) {
    Nova::EnsureMacOSApplicationReady();
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = title ? ToNSString(title) : @"Open File";
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.allowedContentTypes = nil;
        if (@available(macOS 11.0, *)) {
            // UTType not required; extension filter via allowedFileTypes still works on 11+
        }
        panel.allowedFileTypes = BuildAllowedTypes(filters);
        if ([panel runModal] != NSModalResponseOK) {
            return std::nullopt;
        }
        return PathFromNSURL(panel.URL);
    }
}

std::optional<std::filesystem::path> ShowSaveFileDialog(const char* title,
                                                        const std::filesystem::path& defaultPath,
                                                        const std::vector<FileFilter>& filters) {
    Nova::EnsureMacOSApplicationReady();
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.title = title ? ToNSString(title) : @"Save File";
        panel.allowedFileTypes = BuildAllowedTypes(filters);
        if (!defaultPath.empty()) {
            panel.directoryURL =
                [NSURL fileURLWithPath:ToNSString(defaultPath.parent_path().string())];
            panel.nameFieldStringValue = ToNSString(defaultPath.filename().string());
        }
        if ([panel runModal] != NSModalResponseOK) {
            return std::nullopt;
        }
        return PathFromNSURL(panel.URL);
    }
}

} // namespace Nova::Editor
