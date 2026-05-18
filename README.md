# FallingSand
A simple falling sand simulation in C++ with SFML for rendering.

Default sim and frame updates are 60 times a second at 1920x1080, but sim can be a different ratio of 1:1 to pixel display grid.

Render loop is as follows:
- polling of events
- sim orchestrator executes next tick:
  - only if next tick is expected (enough time has passed)
  - if things changed, frame result is updated
  - sim logic is using dirty cells to determine what cells to check/update
- inputs are parsed:
  - left click spawns sand ar cursor location
  - 'R' key resets scene
- frame is displayed

# Screenshot
<img width="720" height="405" alt="image" src="https://github.com/user-attachments/assets/654c5f88-b0d1-4f75-99f8-0868ad484261" />

# TODOs
- Add Linux/MacOS build support via Makefile
- Add more particle types, like water, rock, wood, fire, etc
- Add multithreaded support for sim logic
- Profile SFML to see if replacement is needed
- More complex interactions between materials (change logic from state machine)
