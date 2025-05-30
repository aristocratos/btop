#import <ScreenSaver/ScreenSaver.h>
#import <OpenGL/gl.h>

// For now, create a simple screensaver wrapper
// This can be extended to use the full btop-gl functionality

@interface BtopScreensaverView : ScreenSaverView
{
}
@end

@implementation BtopScreensaverView

- (instancetype)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview
{
    self = [super initWithFrame:frame isPreview:isPreview];
    if (self) {
        [self setAnimationTimeInterval:1/60.0]; // 60 FPS
    }
    return self;
}

- (void)startAnimation
{
    [super startAnimation];
}

- (void)stopAnimation
{
    [super stopAnimation];
}

- (void)drawRect:(NSRect)rect
{
    // Simple colored background for now
    // TODO: Integrate with btop-gl renderer
    [[NSColor blackColor] set];
    NSRectFill(rect);
    
    // Draw a simple animated element
    static float time = 0.0f;
    time += 0.01f;
    
    NSRect animRect = NSMakeRect(
        rect.size.width * 0.1 + sin(time) * rect.size.width * 0.3,
        rect.size.height * 0.1 + cos(time * 0.7) * rect.size.height * 0.3,
        rect.size.width * 0.2,
        rect.size.height * 0.2
    );
    
    [[NSColor colorWithRed:0.2 green:0.8 blue:0.2 alpha:0.8] set];
    NSRectFill(animRect);
}

- (void)animateOneFrame
{
    [self setNeedsDisplay:YES];
}

- (BOOL)hasConfigureSheet
{
    return NO;
}

- (NSWindow*)configureSheet
{
    return nil;
}

@end 