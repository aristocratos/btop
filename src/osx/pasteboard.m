#include <AppKit/AppKit.h>

bool pcopy(const char *buf) {
  NSString *text = @(buf);
  [[NSPasteboard generalPasteboard] clearContents];
  [[NSPasteboard generalPasteboard] setString:text
                                      forType:NSPasteboardTypeString];
  return true;
}
