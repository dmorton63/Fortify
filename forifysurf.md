I like this exercise because you're not asking:

"How do I make windows?"

You're asking:

"What is the fundamental desktop object in Fortify?"

That's a much more important question.

Fortify Surface Architecture v1

Instead of:

Plain Text
Desktop
↓
Windows
↓
Applications

Fortify becomes:

Plain Text
Desktop Surface
↓
Application Surfaces
↓
Applications

A surface is the primary UI object.

Not a window.

Core Concepts
Desktop Surface

The root object.

Plain Text
Desktop Surface

Responsibilities:

Wallpaper/background
Layout management
Notifications
Surface placement
Focus routing
Global shortcuts

Think:

Plain Text
The world applications live in

not:

Plain Text
The thing behind windows
Application Surface

An application never creates a window.

It requests:

C++
SurfaceId CreateSurface();

The compositor owns it.

Example:

Plain Text
Terminal Surface
File Manager Surface
Browser Surface
Text Editor Surface

Each surface contains:

C++
position
size
state
renderer
title
capabilities
Surface States

Instead of:

Plain Text
Windowed
Maximized
Minimized

Fortify surfaces can be:

Plain Text
Floating
Docked
Anchored
Fullscreen
Collapsed
Background
Modal
Panel

Example:

Plain Text
Terminal Surface

might be:

Plain Text
Docked Bottom
Surface Types
Document Surface
Plain Text
README.TXT
Plain Text
CONFIG.TXT
Plain Text
note.txt

Used for content.

Workspace Surface

Contains multiple child surfaces.

Example:

Plain Text
Development Workspace

contains:

Plain Text
Editor
Terminal
Diagnostics
Dashboard Surface

Fortify-specific concept.

Shows:

Plain Text
CPU
Memory
Network
Filesystem
FPS
AP Activity

Built directly into the OS.

Tool Surface

Small utilities.

Example:

Plain Text
Calculator
Color Picker
Music Control
Clock
Layout Engine

The compositor owns placement.

Applications never do.

Applications simply say:

C++
CreateSurface("Terminal");

Compositor decides:

Plain Text
Position
Size
Layer
Animation
Effects
Layers
Plain Text
Layer 0 Desktop
Layer 1 Workspace
Layer 2 Applications
Layer 3 Panels
Layer 4 Notifications
Layer 5 System Alerts

No traditional Z-order mess.

Surface Communication

Since you're already building ports and subsystem messaging:

Plain Text
File Manager Surface
↓
Port
↓
Editor Surface

Open:

Plain Text
README.TXT

becomes:

Plain Text
Message:
OpenFile

rather than window messages.

Rendering Model

Application:

Plain Text
Render into buffer

Compositor:

Plain Text
Compose surfaces

Display:

Plain Text
Backbuffer
↓
Present

This fits perfectly with the double-buffer system you already have.

Long-Term Evolution

This is the part I think you'd like.

A surface can look like a traditional window:

Plain Text
+---------------------+
| README.TXT |
+---------------------+
| |
+---------------------+

or a tile:

Plain Text
┌─────────┬─────────┐
│ Files │ Editor │
├─────────┼─────────┤
│ Console │ Health │
└─────────┴─────────┘

or a dashboard card:

Plain Text
CPU 12%
Memory 2.1 GB
AP Queue 0
FPS 100

The application doesn't care.

Because it owns a:

Plain Text
Surface

not a:

Plain Text
Window
Fortify v1 Rule

If I were writing the spec today, I'd make one simple rule:

Plain Text
Applications never create windows.
 
Applications create surfaces.
 
The compositor decides how surfaces appear.

That's simple enough to implement now, fits naturally with your compositor work, and leaves the door open for traditional windows, tiling layouts, dashboards, or something entirely unique later.