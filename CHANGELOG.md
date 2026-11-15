# Changelog

[Español más abajo](#español)

## Unreleased

- **Multiplayer, laid out as MHGU's Network tab:** *Local play and hubs* (**Search hosts**, **Internet hubs**
  with **Hub servers**, **Matchmaking key**, **Check hub server**), *Play online with* (**This app's hubs**, **Citra Room**,
  **Mixed**) and **Citra Room** (its **Citra Nickname**, **Citra Room Browser**, server, password, and **Citra Web API URLs**: up to 8,
  two come with the app). Hub servers: up to 8, with Add and Remove.
- **Search hosts** (a VPN's hosts) are asked too by *Search this Wi-Fi*.
- **Normal text fields:** everything the settings ask you to type (nickname, address, password, hub servers, web API URLs, the
  matchmaking key...) opens an Android text field with the phone's own keyboard, and its clipboard: long press to paste. The
  app's own on-screen keys are gone.
- **Chat, as AzaharPlus's:** the chat button only opens the chat: an Android bottom sheet over the game with the lines and a text
  field with Send (its **Chat dialog opacity** is in the Chat page). Over the game the lines are plain text with a shadow, top
  left; the Chat page has **Chat text size**, **Chat lines**, **Fade delay** (seconds, 0 = never fade), **Shadow radius**, **Shadow X
  offset**, **Shadow Y offset**, **Background opacity**, **Chat button size** and **Chat button opacity** (which also fades the
  count on the button). The chat's messages and "Say something" are no longer on the Chat page.
- **Hub chat and Mixed chat:** a new **Hub** chat, only for the players of the hub you host or join (over the online chat's
  relays, with a tag only that hub asks for: nobody outside it reads it). With *Play online with* **Mixed** there are two more:
  **Mixed (Room + Hub)**, where what you say goes to the room and the hub, and what the others say in one passes into the other
  (a room's chat carries no other name, so it comes as "Name (Hub): text" under your nickname, and the other way as "Name
  (Room): text"), and **Mixed (All)**, which adds Global to what you say and read (Global is never passed on). Names show as
  "Name (Room)", "Name (Hub)", "Name (MH4U)" or "Name (MHGU)"; your own lines as "You".
- **Touch layout: Adaptive or Traditional** (Controls): Traditional is the emulator-style layout with its own list of buttons
  shown or hidden (all shown at first), a **C-stick**, **Auto run on the joystick** (off at first) and **Camera swipe** (off at first).

## 1.0

Your code library stays valid: there's no need to build it again.

1.0 was tried on a PC and on our phones (Mali-G57, Mali-G52 and Adreno 830), but not every part of it on a phone: where a
line says "should", please tell us how it goes on yours (the Discord or an issue, with the log).

**New in 1.0**

- **A new settings menu:** six sections down the left, **Controls**, **Graphics & Audio**, **Gameplay**, **Multiplayer**,
  **Mods & Cheats** and **System**, with **Credits & Licenses** under them. Each section's page has its main options, and
  the rest is one level down (**Touch controls**, **Controller**, **Screen and HUD**, **Advanced**, **Join a room**...).
  Each row's explanation is behind its **?**. Your options keep their values; only their places changed: **Display** is
  now **Graphics & Audio**, **Screen and HUD**; the **Comforts** are in **Gameplay**; **Menu Options** is **System**
  (with **Your game** for the folders, **Import save**, **Export save** and **Share log**).
- **Build the game's code on the phone, no PC needed:** in the game folder screen, **Build on this phone** when the library
  is missing or from another version, and later gear, **System**, **Game code**, **Build the game code**. It builds in the
  code mod of your `mods` folder (or another, or none), downloads its tools the first time (about 100 MB: Python from
  python.org and Zig from PyPI; never anything of the game), and goes on in the background, slower when the phone is warm
  and paused when it is hot or the battery is low; a cut build goes on from where it stopped: **Go on** goes straight back
  to compiling the files still missing, without analysing and translating the game's code again (within a second on an
  RG406V; on a hot Galaxy S25 Ultra, 71 times within 0-2 s, where translating it all takes about 70 s). A broken entry
  in the compiler's cache is now emptied and compiled again instead of stopping the build (seen and fixed on the Galaxy).
  A library from a build that was cut can differ byte for byte from one made in one go, in one very large function (a
  limit of the compiler), and it runs the same. Inside the app a whole build took 32 minutes on an RG406V (Mali-G57; again
  with this release's own APK) and
  about 92 minutes on a Galaxy S25 Ultra that paused 72 times for heat; both made the same library, and the game runs Val
  Habar at 60 FPS with it. It should take about 2 hours on a low-end phone. Klark231's idea.
- **Single-screen mode of our own:** the touch screen's menus drawn on the top screen by the game itself, in every
  language, Spanish included: the title menu, quest counters and lists, the canteen, shops, Palico menus, help pages and
  yes / no questions. On by default, also when you update (gear, **Graphics & Audio**, **Screen and HUD**); with the USA
  game the touch screen starts hidden and its button brings it back for the town's map, the World Map, the keyboard and
  character creation. It stays off while a code mod is on (jM5557's mod, Heavenfall). Based on Heavenfall's single-screen
  mode, by popzol and BlitzWOLF, without its gameplay changes. Tried in English and Spanish on an RG406V and a Galaxy S25
  Ultra (title, saves, quest counter and lists, START menu, pouch, item box); the canteen and the smithy weren't reached
  on a phone yet.
- **Control profiles:** 10 slots for your touch layouts, each with everything: buttons, HUD, the FPS counter's place, held
  buttons and pictures. Each one is the port's own layout or an **emulator-style** one (A, B, X and Y in a diamond, the
  D-pad, the shoulder buttons, START and SELECT, each pressing only its own button). In the editor, **Profile**, or gear,
  **Controls**, **Control profiles**. Your layout from before is profile 1, unchanged. Designed by tabarong.
- **Buttons that stay pressed** (the editor's **Hold**), **your own pictures on the buttons** (the editor's **Picture**, or
  **Controls**, **Touch controls**, **Button images**), the **FPS counter** dragged anywhere in the editor, and a
  **floating stick**, off by default (**Controls**, **Touch controls**). Tried in the MHGU port on a phone; here they should
  work the same: tell us if something is off.
- **Chat and friends:** gear, **Multiplayer**, **Chat** and **Friends**. The **Room** chat is the room's own (Azahar's
  players read it too); **General** (MH4U) and **Global** (both ports) and private messages with friends go through public
  Nostr relays, only after you turn the online chat on, with no account and no server of ours: you are a friend code made
  on your phone. Mute and block are yours alone, and there is a limit against spam. Both chats start off: until you turn
  on **Room chat** or **Online chat**, at the top of **Chat**, there's no chat button and nothing of the chat over the game,
  and the room's own messages show as before (**Room messages over the game**). With a chat on, a chat button over the game
  shows new lines. Tested between a phone and a PC; between two phones it should work the same. tabarong's design.
- **Room passwords:** under **Join a room**, **Password**, **Generate a password** makes one, and **Share this room in the
  chat** sends the room to a friend, who joins it with one tap. Joining a room with the room chat on, a notice reminds you
  to go to the Gathering Hall's **Local Play**.
- **Crossplay with Azahar and Citra,** said plainly: the rooms are the same, everyone needs the 1.3.0 update, and a hunter
  in the room playing another game is marked (they can't reach your Gathering Hall). See **Multiplayer**, **Play with
  emulators**.
- **Effects:** gear, **Graphics & Audio**, **Effects** off leaves out the game's particle effects (hits, sparks, fire,
  breath, dust, rain), at once: faster where there are many, but the effects that warn of an attack go too.
- **HD texture packs:** the packs made for Azahar and Citra load from a `textures` folder in your game folder (PNG
  textures). Each texture takes the phone's memory: a big pack can close the game on a phone with little RAM. Tested with a
  test pack on the PC; a real pack should load the same.
- **Clear shader cache** (gear, **Graphics & Audio**, **Advanced**): for a picture that shows something wrong after a GPU
  driver update, the next start compiles them again, and a small plate says how far it is.
- **PowerVR:** when the app has to switch to its slower, safer way of showing the screens, a notice says why the frame rate
  dropped.
- **3D resolution:** its help says why there is no 55% or exact size (the picture can only be a whole number of times the
  console's screen) and that the HUD is as sharp as this resolution.
- **A keyboard and a mouse on the phone:** while both are connected, **Controls**, **Keyboard and mouse**, **Keybinds**
  changes every key, and a left click on the game's picture captures the mouse to turn the camera (Escape lets it go); a
  click on the touch screen or on a touch control is still a tap there. Tried on an RG406V with a virtual keyboard and
  mouse; a real mouse in a quest should work the same.
- **Bows and bowguns with the lock-on:** with the lock-on on, a bow's draw and a bowgun's shot turn the hunter to the
  monster followed, as in the MHGU port. It should help; not yet tried in a hunt with a monster.
- **No more double tap on the camera:** an accidental one in a fight turned the camera away. The camera button (L) still
  centres it.
- **Menus:** two options in **Buttons on the screen**: **L and R in menus** (the menu pad's tabs, on) and **D-pad in menus**
  (off: the menu pad takes its place, as before).
- **Controllers:** a controller that sends A, B, X and Y without Android listing them is added again at its first press,
  so the game reads it, as in the MHGU port; and the log lists every input device.
- **Send all to the box:** at the end of a quest, the game's **Send to Item Box** sends every reward at once; what doesn't
  fit stays in the list. Off by default: gear, **Gameplay**, **Send all to the box**. Tried in a low-rank quest on an
  RG406V. Alfonso's idea.
- **Import DLC and event quests** (gear, **Mods & Cheats**, **DLC and event quests**): bring the event quests you already
  have, from an emulator or a backup of your console. The app includes none: you bring your own, at your own risk.
- **Home screen shortcut** (gear, **System**): the game on your home screen with a picture and a name you choose.
- **Verify game files** (gear, **System**, **Your game**): reads the game and its update whole and says plainly what is
  missing, cut short, changed or from another version or region (USA 1.3.0, Europe 1.1.0, the base game alone, MH4G),
  whether the copy looks modified (a repack), whether the code library and its `.id` fit this version and come from the
  same build, which code mods are on or clash, whether the save has its extra data, and what to do. It runs in the
  background with its progress and **Cancel**, and the full list goes to the log for whoever helps you. It changes and
  downloads nothing: only checksums ship with the app.
- **Mods:** code mods that don't go together are named on the **Mods** page; the error screen says plainly when a library
  comes from another tool, from two different builds (`.so` and `.id`) or from another build of a mod; a code mod's own
  `exheader.bin` is used. Files and folders spelled with other capitals (`Data` for `data`) now replace the game's, and
  **Add a mod** finds a mod deeper inside a zip (also Luma's `luma/titles` layout). The game folder's `mods` is searched
  as deep too: a mod found there only this way is added off, and the **Mods** page says so; turn it on there.
- **Heavenfall** should now build as an optional mod you get yourself, from its 1.3.0 update route and from its web
  installer's 1.0 route. It was only tried with a stand-in we made, never with the real mod: tell us. Its layered armour
  comes with it. Export your save before you try it.
- **Cheats as the lists write them:** tabs, codes written together, comments after the codes, CTRPF notes and files with a
  UTF-8 mark load now. A cheat that writes into the game's code can be made a code mod (**Cheats**, select it, **Make a code
  mod**), built into your library.
- **The European game (1.1.0)** should work too, with its own mods and cheats (we have no European copy to try it).
  **MH4G** is recognized but not supported yet.

Thanks to tabarong for the designs of the control profiles, the chat and the room passwords, to Klark231 for the idea of
building the game's code on the phone, to popzol and BlitzWOLF for Heavenfall's single-screen mode, the base of ours,
and to Alfonso for sending all the rewards at once.

**Fixes and more**

- **Ammo with L held:** a step of the ammo or the coating with **L** held should keep the ammo you chose again (it went back
  to the one before; seen on a Moto g06 with a light bowgun, Discord).
- **Lock-on in another area:** it stays on with no monster to follow (another area, a cart to camp) and follows again once
  one is back, as in the MHGU port; the button, a controller's right stick click and the quest's end still turn it off
  (seen on a Moto g06).
- **A Lock-on button of its own:** in a quest, a tap on the new **Lock-on** button (left of the camera button, L) keeps
  the camera turning to the monster until you tap it again, as in the MHGU port. It presses none of the game's buttons,
  so holding **L** works as the game's **L** again, for example **L** + **X** / **B** to change a bowgun's ammo (Discord).
  Move it in the layout editor, or hide it: gear, **Controls**, **Buttons on the screen**, **Lock-on button**. A
  controller still locks on with the right stick's click.
- **Keyboard:** played by touch, the menu's keyboard opens with your phone's own keyboard already up, as in the MHGU port.
- **Samsung DeX** ([#40](https://github.com/khorne-hub/4u-native/issues/40)): maximizing the game's window restarted the
  game over and over. The window can now change size without a restart, which should fix it (we have no DeX device to
  try it on: please tell us).
- **Adreno 610 stuck at 0 FPS** ([#47](https://github.com/khorne-hub/4u-native/issues/47)): Adreno GPUs with older
  Vulkan 1.1 drivers show the picture the way 0.1.9 did again (0.1.9.1 had changed it for them). And if the phone's GPU
  driver stops answering, the game no longer freezes: it closes with the error screen, where you can share the log.
- **Coloured streaks on an Adreno 619** (Moto G34, Discord): the 3x minimum of the 3D resolution is back for that GPU
  with its system driver, which should remove them. **Adreno fix** turns it off.
- **PowerVR** ([#48](https://github.com/khorne-hub/4u-native/issues/48), Discord): the shaders you've already seen are
  built again in the background at the start, one at a time, as in the MHGU port, so the short stops at each new effect
  should get much rarer after your first session. A PowerVR BXM that closed at every start should start now. Older
  PowerVR GPUs that showed a black screen with sound now draw the 3D at the most their GPU can: the 3D resolution row
  says so, for example "100 % (5x) · 4x".
- **Long sessions on a PowerVR BXM** ([#31](https://github.com/khorne-hub/4u-native/issues/31)): `log.txt` now names
  what keeps growing until the game closes, and close to the system's limit the app switches to a slower but safer way of
  showing the screens. This may help; a log of a long session will tell us more.
- **HUD at the screen's edges:** the weapon gauges under the sharpness (a Charge Blade's phials, the other weapons'
  gauges), the Frenzy and a bowgun's ammo now move with the rest of the HUD (Discord).
- **More in the log:** after a crash inside the GPU driver, the thread's name and the system's own error lines (Discord);
  the performance line each minute now has the phone's temperature status and CPU speed, to tell whether a slowdown
  comes from the phone's heat ([#50](https://github.com/khorne-hub/4u-native/issues/50), Discord); and a picture shown
  for one wrong frame leaves lines in the log (Discord).
- **The library builder on PC:** its window no longer closes at once. It says when the download wasn't unzipped first,
  and always waits so you can read its last lines (Discord).
- **Pickers' titles:** the title of a window to choose from fits whole on a 4:3 screen, its diamond and **?** included:
  drawn smaller when it has to, instead of cut.
- **The gear and the game's menus:** with single-screen mode (or a code mod's) on a 4:3 screen, the settings gear waits in
  the bottom-left corner while a menu of the game's is on the top screen, instead of covering it; a gear you moved stays
  where you put it.
- **The touch buttons and the game's menus:** with single-screen mode (or a code mod's), while a menu of the game's is on
  the top screen (the item box, the pouch, a shop), a touch button with half its picture or more over it is drawn faint,
  so the whole menu reads; on a 20:9 phone the right-hand buttons covered the item box's last column. They still work as
  always, and nothing moves.
- **Import save from a PC** (Discord): a save from Azahar, Citra, Checkpoint or JKSM comes in from whatever folder you
  pick: the emulator's whole folder, its `sdmc`, the save's `data` or title folder, or the backup tool's folder. A
  European save went where the USA game never looks, so your hunter didn't show: a save from the other region (or from
  the Japanese MH4G) is now refused with a message that names both games, and so are a hunter's file cut short and one
  without its `system` file when you have no save yet. The guild cards, event quests and Palico next to an emulator's
  save come along too (they never did). After the restart a notice says what came in, and **Share log** includes the
  last import. We couldn't make the black screen after an import happen here: if you still get it, please send the log.
- **README:** the setup in short, the tool from the **Source code (zip)** of the same release as your APK, how to switch
  from a copy from another website without losing your save, more rows in *If something goes wrong*, the Lock-on
  button, and for 1.0: building the game's code on the phone, single-screen mode, play online step by step, chat and
  friends, crossplay, the event quests' import, keeping your save in the cloud with a sync app, HD packs and the new
  menu's places.

## 0.1.9.2

Your code library stays valid: there's no need to build it again.

- **Combined buttons on a controller:** each combined button can go on a controller's button (L3, R3, SELECT, Guide,
  Misc, the paddles or the touchpad): gear, **Controls**, **Controller**, **Combined buttons**, a row that shows while a
  controller is connected. A player's suggestion on Discord.
- **More in the log for slow phones** ([#43](https://github.com/khorne-hub/4u-native/issues/43)): `log.txt` now tells how
  far the game's list of shaders has got while it is being built, and how long it took, and has a performance line once a
  minute.
- **The single-screen mod** is made for the USA game in English: with the game in another language its menus overlap
  ([#30](https://github.com/khorne-hub/4u-native/issues/30)). The README says so now.

## 0.1.9.1

Your code library stays valid: there's no need to build it again.

- **Fixed the flicker with the touch screen hidden** (bright, dark or broken frames every few seconds that stopped when
  the touch screen showed) on GPUs with older graphics drivers, such as Mali-G71 and G72, and on PowerVR: the top screen
  now always reaches the display the same way there. Mali and Adreno GPUs with recent drivers are unchanged.

## 0.1.9

Your code library stays valid: there's no need to build it again.

- **Run with the stick:** push it past its edge to run, and lift your thumb on the padlock circle above it to keep running;
  touching the stick or the circle again stops. The circle moves and resizes in the layout editor. Turned off, a **Run**
  button runs while you hold it.
- **Bows and bowguns:** a tap on aim keeps the aim up until the next tap; hold and slide to aim. **Aim with the gyro**
  (off by default), with its sensitivity, turns the aim as you turn the phone.
- **Menu pad:** in the game's menus a small pad comes in under your right thumb, with L and R above it, in place of the small
  menu D-pad. Quest rewards and the Reward Box count as menus, and the hunt D-pad steps aside while the pad shows.
- **Lock-on:** the padlock by the target icon, which didn't respond, is gone; a gold ring round the icon shows the lock-on.
- **Black screen with the menu's music on some older Mali GPUs:** when the phone's graphics refuse the usual way of showing
  the picture, the app now switches to another by itself, and `log.txt` tells more about the phone's graphics.
- **Picture:** cutscenes are sharp again; textures the game rewrites are caught (no more outfits with the old pattern in
  character creation, or wrong ground and sky); long sessions use less memory for textures, so they keep loading on 3-4 GB
  phones; a steady 30 fps on phones that switch the screen to 30 Hz.
- **Other GPUs:** older Vulkan 1.1 drivers get shaders they accept; on Adreno the 3x minimum is only for drivers older than
  1.1.128, and **Adreno fix** turns it off; Adreno 610-class phones start with the light settings; PowerVR and other GPUs
  no longer draw over a picture still on the screen.
- **Save folder** (Menu Options): keep your save in a folder a sync app shares with your other devices. MHGU for Android
  can share the same folder.
- **Import save** has **Choose a folder** and finds emulators' folders by itself. An import never deletes your old save
  before the new one is in place.
- **Faster eating and drinking:** two cheats, off, as in the MHGU port. **Comforts** in Cheats, after jM5557's QOL mods:
  hold A to gather, move to skip the flex, B cancels sharpening, no time limit on rewards.
- **Playing together:** a community lobby comes set, the lobbies you type are remembered (the × removes one), and the first
  time online a notice names the server.
- **Playing alone,** the game waits while the settings or the layout editor are open.
- **GitHub and Discord buttons** beside Save open the project's page and the community's Discord (they ask first).
- **Notices:** at most once a day the app checks for a new version (**Check for updates** turns it off), and a copy that
  isn't the official one says so.
- Also: dashing and tapping the camera in a village no longer opens the single-screen mod's World Map (L and R together
  still do); a thumb landing on the stick takes the stick; the error screen's way back to the last start's mods works with
  one tap, also for a code mod the app can't load; the layout editor's panel moves and gets smaller; a 240 fps choice.

## 0.1.8

- **Fixed a strip of wrong colours along the top edge** at low 3D resolutions (Low-end mode, 50 %): the game never draws the
  top screen's first row, which showed as a whole line at 1x.

## 0.1.7

- **Fixed a black screen with "No game folder at /game"** on phones where Android gives the app no external folder (for
  example a Redmi Note 8): the game now reads its setup from the same folder the folder screen writes it to.
- **Adreno GPUs with a Vulkan 1.1 driver** (for example the Galaxy S9's Adreno 630): their driver misdraws the 3D picture
  below 720 lines (coloured bands), so there the 3D resolution is 3x at least.

## 0.1.6

- **PowerVR GPUs:** 0.1.4 played, but the next start stopped again: the GPU's shader compiler also crashed when the app built
  its saved shaders on a second thread. Now every shader is built on one thread when first needed.

## 0.1.5

- **Fixed the combined buttons page** (gear, Controls, buttons on the screen, Button 1-6): it showed nothing.

## 0.1.4

- **PowerVR GPUs** (for example the GE8300 of the Helio A22): the game stopped a few seconds after start, when the GPU's
  own shader compiler crashed on the app's largest shader. That shader is no longer built there; each picture's shaders
  are built when first needed, so a new effect may hitch once. Untested on a PowerVR phone: please report how it goes.

## 0.1.3

- **Fixed the crash at start that 0.1.2 still had on some phones whose graphics driver is Vulkan 1.1** (for example
  Mali-G57 phones with a Unisoc chip). Your code library stays valid.

## 0.1.2

- **Fixed a crash right after start on phones whose graphics driver is Vulkan 1.1** (for example some Adreno 730 phones):
  0.1.1 got past the black screen, then stopped at the first frame. Your code library stays valid.
- **Fixed the game stopping on the first start on some Android 14 phones** (for example a Moto g55): it couldn't make its
  save folders.

## 0.1.1

- **Fixed a black screen on start on Adreno 7xx phones** (for example the Snapdragon 8+ Gen 1's Adreno 730): their
  graphics driver was refused at start. Your code library stays valid: no need to build it again.

## 0.1.0

The first public version: Monster Hunter 4 Ultimate (USA, 1.3.0) recompiled to native code for Android, with your own
copy of the game.

- **Native, not emulated:** the game's code is recompiled on your PC into a library for your phone, and runs on a
  runtime of its own, with the 3DS's graphics drawn through Vulkan.
- **Your phone's resolution** by default, widescreen that matches your screen (4:3 handhelds too, no bars), and a frame
  rate limit from 30 to 240, with an optional steady frame rate.
- **Low-end mode** and **Toaster mode** for weak phones and long hunts on one charge; turned on by itself the first time on
  a weak phone.
- **Touch controls made for phones:** lock-on with the padlock (the camera follows the monster) and the target button to change monster, the item bar by tap and swipe without the D-pad, a layout of their own for
  bows and bowguns, combined buttons, and a layout editor that also moves the game's own HUD.
- **Controllers and keyboards:** the right stick as the camera, button mapping per controller, START+SELECT for the
  settings, a full keyboard layout.
- **Town actions without the touch screen:** Guild Cards, Gestures, Friends and Hunters for Hire from a list (a touch
  button in town, SELECT on a controller); what they open shows large in the middle, with a cursor for the controller.
- **Menus with your own layout:** the controls don't switch when a menu opens. A small D-pad shows only while a menu
  needs it (item boxes, shops, the quest board, the START menu…), and your buttons fade to half so the menu stays
  readable. Both can be placed, sized or turned off.
- **Item bar arrows:** a tap moves one item, holding repeats; they follow the item bar wherever you put it. Swiping still
  works.
- **Quick taps always count:** a tap on the touch screen is held until the game has seen it.
- **Credits & Licenses:** the credits when the app opens, a one-time thank-you, and a page with the credits, the legal
  notice and every license.
- **The single-screen mod** works (get it from its author; it's built into your library).
- Mods with load order and conflicts, cheats in Azahar's format, network rooms and rooms on the same Wi-Fi, save import and
  export, your own GPU driver on Adreno, an FPS counter with the temperature, and an error screen that shares the log.
- Android 9 and newer, 64-bit.

### Known

- The single-screen mod's big area map (shown after you take the Map from the supply box) can't be moved yet.
- In town, SELECT opens the town actions instead of the game's Customize Panels; set "Town actions button" to another
  button or off in Controls to get it back.
- With a power-saving game profile, some phones hold about 105-110 FPS instead of 120 in the field; 60 is steady.

### Official downloads

MH4U for Android is free. Only the links khorne-hub gives are official: if you got it anywhere else, paid for it, or it
came with ads, it isn't the official one. Check the APK's SHA-256 against the release's `.sha256` file.

## Español

## 1.0

Tu librería de código sigue sirviendo: no hace falta crearla de nuevo.

La 1.0 se probó en una PC y en nuestros teléfonos (Mali-G57, Mali-G52 y Adreno 830), pero no todo en un teléfono: donde una
línea dice "debería", cuéntanos cómo te va en el tuyo (en el Discord o en un issue, con el registro).

**Lo nuevo de la 1.0**

- **Un menú de opciones nuevo:** seis secciones a la izquierda, **Controles**, **Gráficos y audio**, **Juego**,
  **Multijugador**, **Mods y trucos** y **Sistema**, con **Créditos y licencias** debajo. La página de cada sección tiene sus
  opciones principales, y el resto está un nivel más abajo (**Controles táctiles**, **Mando**, **Pantalla y HUD**,
  **Avanzado**, **Entrar a una sala**...). La explicación de cada fila está detrás de su **?**. Tus opciones conservan sus
  valores; solo cambiaron de lugar: **Pantalla** ahora es **Gráficos y audio**, **Pantalla y HUD**; las **Comodidades** están
  en **Juego**; **Opciones del menú** es **Sistema** (con **Tu partida** para las carpetas, **Importar partida**,
  **Exportar partida** y **Compartir registro**).
- **Crear el código del juego en el teléfono, sin PC:** en la pantalla de la carpeta del juego, **Crear en este teléfono**
  cuando falta la librería o es de otra versión, y después en el engranaje, **Sistema**, **Código del juego**, **Crear el
  código del juego**. Incluye el mod de código de tu carpeta `mods` (u otro, o ninguno), descarga sus herramientas la
  primera vez (unos 100 MB: Python desde python.org y Zig desde PyPI; nunca nada del juego) y sigue en segundo plano, más
  despacio cuando el teléfono está caliente y en pausa cuando está muy caliente o con poca batería; si se corta, sigue desde
  donde quedó: **Seguir** vuelve directo a compilar los archivos que faltan, sin volver a analizar ni traducir el código
  del juego (en menos de un segundo en una RG406V; en un Galaxy S25 Ultra caliente, 71 veces en 0-2 s, donde traducirlo
  todo tarda unos 70 s). Una entrada rota en la caché del compilador ahora se vacía y se vuelve a compilar en vez de
  frenar el armado (visto y arreglado en el Galaxy). Una librería de un armado que se cortó puede ser distinta byte a byte
  de una hecha de una vez, en una función muy grande (un límite del compilador), y anda igual. Dentro de la app un armado
  entero tardó 32 minutos en una RG406V (Mali-G57; de nuevo con la APK de esta versión) y unos 92 minutos en un Galaxy S25
  Ultra que paró 72 veces por calor;
  los dos crearon la misma librería, y con ella el juego anda en Val Habar a 60 FPS. En un teléfono de gama baja debería
  tardar unas 2 horas. Idea de Klark231.
- **Un modo de pantalla única propio:** los menús de la pantalla táctil dibujados en la de arriba por el propio juego, en
  todos los idiomas, español incluido: el menú de título, los mostradores y listas de misiones, la cantina, las tiendas, los
  menús de los Felyne, las páginas de ayuda y las preguntas de sí o no. Activado por defecto, también al actualizar
  (engranaje, **Gráficos y audio**, **Pantalla y HUD**); con el juego de EE. UU. la pantalla táctil arranca oculta y su
  botón la vuelve a mostrar para el mapa del pueblo, el mapa del mundo, el teclado y la creación del personaje. Queda
  apagado mientras haya un mod de código activo (el mod de jM5557, Heavenfall). Basado en el modo de una pantalla de
  Heavenfall, de popzol y BlitzWOLF, sin sus cambios al juego. Probado en inglés y en español en una RG406V y un Galaxy
  S25 Ultra (título, partidas, mostrador y listas de misiones, menú START, bolsa, caja de objetos); la cantina y la
  herrería todavía no se probaron en un teléfono.
- **Perfiles de controles:** 10 espacios para tus diseños táctiles, cada uno con todo: botones, HUD, el lugar del contador
  de FPS, los botones que quedan apretados y las imágenes. Cada uno es el diseño propio del port o uno **de emulador** (A,
  B, X e Y en rombo, la cruceta, los gatillos, START y SELECT, cada uno aprieta solo su botón). En el editor, **Perfil**, o
  engranaje, **Controles**, **Perfiles de controles**. Tu diseño de antes es el perfil 1, sin cambios. Diseño de tabarong.
- **Botones que quedan apretados** (**Mantener** en el editor), **tus propias imágenes en los botones** (**Imagen** en el
  editor, o **Controles**, **Controles táctiles**, **Imágenes de los botones**), el **contador de FPS** arrastrado a
  cualquier lugar en el editor, y una **palanca flotante**, desactivada por defecto (**Controles**, **Controles
  táctiles**). Probado en el port de MHGU en un teléfono; aquí debería funcionar igual: avísanos si algo no anda bien.
- **Chat y amigos:** engranaje, **Multijugador**, **Chat** y **Amigos**. El chat **Sala** es el propio de la sala (también lo
  leen los jugadores de Azahar); **General** (MH4U) y **Global** (los dos ports) y los mensajes privados con amigos pasan por
  relés públicos de Nostr, solo después de que actives el chat en línea, sin cuenta ni servidor nuestro: eres un código de
  amigo creado en tu teléfono. Silenciar y bloquear son solo tuyos, y hay un límite contra el spam. Los dos chats empiezan
  apagados: hasta que actives **Chat de la sala** o **Chat en línea**, arriba en **Chat**, no hay botón del chat ni nada del
  chat sobre el juego, y los mensajes propios de la sala se ven como antes (**Mensajes de la sala sobre el juego**). Con un
  chat activo, un botón de chat sobre el juego muestra las líneas nuevas. Probado entre un teléfono y una PC; entre dos
  teléfonos debería funcionar igual.
  Diseño de tabarong.
- **Contraseñas de sala:** en **Entrar a una sala**, **Contraseña**, **Generar una contraseña** crea una, y **Compartir esta
  sala en el chat** le manda la sala a un amigo, que entra con un toque. Al entrar a una sala con el chat de la sala activo,
  un cartel te recuerda ir al **Local Play** del Gathering Hall.
- **Juego cruzado con Azahar y Citra,** dicho claro: las salas son las mismas, todos necesitan la actualización 1.3.0, y un
  cazador de la sala que juega otro juego aparece marcado (no puede llegar a tu Gathering Hall). Mira **Multijugador**,
  **Jugar con emuladores**.
- **Efectos:** engranaje, **Gráficos y audio**, **Efectos** apagado deja afuera los efectos de partículas del juego (golpes,
  chispas, fuego, alientos, polvo, lluvia), al momento: más rápido donde hay muchos, pero también desaparecen los efectos que
  avisan de un ataque.
- **Packs de texturas HD:** los packs hechos para Azahar y Citra cargan desde una carpeta `textures` en la carpeta de tu
  juego (texturas PNG). Cada textura ocupa memoria del teléfono: un pack grande puede cerrar el juego en un teléfono con
  poca RAM. Probado con un pack de prueba en la PC; un pack real debería cargar igual.
- **Borrar la caché de shaders** (engranaje, **Gráficos y audio**, **Avanzado**): para una imagen que muestra algo mal
  después de actualizar el driver de la GPU, el próximo inicio los vuelve a compilar, y un cartel chico dice cuánto falta.
- **PowerVR:** cuando la app tiene que pasar a su forma más lenta y segura de mostrar las pantallas, un cartel dice por qué
  bajaron los cuadros por segundo.
- **Resolución 3D:** su ayuda explica por qué no hay 55 % ni tamaño exacto (la imagen solo puede ser un número entero de
  veces la pantalla de la consola) y que el HUD es tan nítido como esa resolución.
- **Un teclado y un ratón en el teléfono:** mientras los dos están conectados, **Controles**, **Teclado y ratón**,
  **Asignación de teclas** cambia cada tecla, y un clic izquierdo en la imagen del juego captura el ratón para girar la
  cámara (Escape lo suelta); un clic en la pantalla táctil o en un control táctil sigue siendo un toque ahí. Probado en una
  RG406V con un teclado y un ratón virtuales; un ratón real en una misión debería andar igual.
- **Arcos y ballestas con el fijado:** con el fijado activado, tensar el arco y disparar la ballesta giran al cazador hacia
  el monstruo que sigue, como en el port de MHGU. Debería ayudar; todavía sin probar en una cacería con un monstruo.
- **Se acabó el doble toque en la cámara:** uno sin querer en plena pelea la giraba. El botón de la cámara (L) la sigue
  centrando.
- **Menús:** dos opciones en **Botones en la pantalla**: **L y R en los menús** (las pestañas del pad de menú, activadas) y
  **Cruceta en los menús** (desactivada: el pad de menú ocupa su lugar, como antes).
- **Mandos:** un mando que manda A, B, X e Y sin que Android los liste se vuelve a agregar con su primer botón, así el juego
  lo lee, como en el port de MHGU; y el registro lista todos los dispositivos de entrada.
- **Enviar todo a la caja:** al terminar una misión, la opción del juego de enviar a la caja de objetos manda todas las
  recompensas de una vez; lo que no entra queda en la lista. Apagado por defecto: engranaje, **Juego**, **Enviar todo a la
  caja**. Probado en una misión de rango bajo en una RG406V. Idea de Alfonso.
- **Importar DLC y misiones de evento** (engranaje, **Mods y trucos**, **DLC y misiones de evento**): trae las misiones de
  evento que ya tienes, de un emulador o de una copia de tu consola. La app no incluye ninguna: las traes tú, bajo tu
  responsabilidad.
- **Acceso directo en el inicio** (engranaje, **Sistema**): el juego en tu pantalla de inicio con la imagen y el nombre que
  elijas.
- **Verificar archivos del juego** (engranaje, **Sistema**, **Tu partida**): lee el juego y su actualización enteros y dice
  claro qué falta, qué está cortado, qué cambió o es de otra versión o región (USA 1.3.0, Europa 1.1.0, el juego base solo,
  MH4G), si la copia parece modificada (un repack), si la librería del código y su `.id` corresponden a esta versión y son
  del mismo armado, qué mods de código están activos o chocan, si la partida tiene sus datos extra, y qué hacer. Corre de
  fondo con su progreso y **Cancelar**, y la lista completa va al registro para quien te ayude. No cambia ni descarga
  nada: con la app solo van sumas de control.
- **Mods:** la página **Mods** nombra los mods de código que no van juntos; la pantalla de error dice claro cuando una
  librería viene de otra herramienta, de dos armados distintos (`.so` e `.id`) o de otro armado de un mod; se usa el
  `exheader.bin` propio de un mod de código. Los archivos y carpetas escritos con otras mayúsculas (`Data` por `data`) ahora
  reemplazan los del juego, y **Agregar un mod** encuentra el mod más adentro de un zip (también con la forma
  `luma/titles` de Luma). La carpeta `mods` de la carpeta del juego se busca igual de adentro: un mod que solo se encuentra
  así entra desactivado, y la página **Mods** lo dice; actívalo ahí.
- **Heavenfall** debería poder crearse ahora como un mod opcional que consigues tú, por su ruta de la actualización 1.3.0 y
  por la ruta 1.0 de su instalador web. Solo se probó con un sustituto hecho por nosotros, nunca con el mod real:
  cuéntanos. Su armadura de capas viene con él. Exporta tu partida antes de probarlo.
- **Trucos como los escriben las listas:** tabulaciones, códigos escritos juntos, comentarios después de los códigos, notas
  de CTRPF y archivos con marca UTF-8 ahora cargan. Un truco que escribe en el código del juego se puede convertir en un mod
  de código (**Trucos**, selecciónalo, **Hacer un mod de código**), que se incluye en tu librería.
- **El juego europeo (1.1.0)** debería funcionar también, con sus propios mods y trucos (no tenemos una copia europea para
  probarlo). **MH4G** se reconoce pero todavía no se puede usar.

Gracias a tabarong por los diseños de los perfiles de controles, el chat y las contraseñas de sala, a Klark231 por la idea
de crear el código del juego en el teléfono, a popzol y BlitzWOLF por el modo de una pantalla de Heavenfall, la base del
nuestro, y a Alfonso por mandar todas las recompensas de una vez.

**Arreglos y más**

- **Munición con L mantenida:** un paso de la munición o de la capa con **L** mantenida debería volver a quedarse con la
  munición que elegiste (volvía a la anterior; visto en un Moto g06 con una ballesta ligera, Discord).
- **Fijar objetivo en otra zona:** se queda activado sin un monstruo que seguir (otra zona, un carro al campamento) y vuelve
  a seguir apenas aparece uno, como en el port de MHGU; el botón, el clic de la palanca derecha de un mando y el final de la
  misión lo siguen apagando (visto en un Moto g06).
- **Un botón propio para fijar objetivo:** en una misión, un toque en el botón nuevo **Fijar objetivo** (a la izquierda
  del botón de la cámara, L) hace que la cámara siga girando hacia el monstruo hasta que lo vuelvas a tocar, como en el
  port de MHGU. No aprieta ningún botón del juego, así que mantener **L** vuelve a funcionar como la **L** del juego, por
  ejemplo **L** + **X** / **B** para cambiar la munición de una ballesta (Discord). Se mueve en el editor del diseño, o se
  oculta: engranaje, **Controles**, **Botones en la pantalla**, **Botón de fijar objetivo**. Con un mando se sigue
  fijando con el clic de la palanca derecha.
- **Teclado:** jugando con la pantalla táctil, el teclado del menú se abre con el teclado de tu teléfono ya visible, como
  en el port de MHGU.
- **Samsung DeX** ([#40](https://github.com/khorne-hub/4u-native/issues/40)): al maximizar la ventana, el juego se
  reiniciaba una y otra vez. Ahora la ventana puede cambiar de tamaño sin reiniciar, lo que debería arreglarlo (no
  tenemos un equipo con DeX para probarlo: cuéntanos).
- **Adreno 610 trabado en 0 FPS** ([#47](https://github.com/khorne-hub/4u-native/issues/47)): las GPU Adreno con drivers
  Vulkan 1.1 más viejos vuelven a mostrar la imagen como en la 0.1.9 (la 0.1.9.1 lo había cambiado para ellas). Y si el
  driver de la GPU del teléfono deja de responder, el juego ya no se congela: se cierra con la pantalla de error, desde
  donde puedes compartir el registro.
- **Rayas de colores en un Adreno 619** (Moto G34, Discord): vuelve el mínimo de 3x de la resolución 3D para esa GPU con
  su driver del sistema, lo que debería quitarlas. **Corrección de Adreno** lo desactiva.
- **PowerVR** ([#48](https://github.com/khorne-hub/4u-native/issues/48), Discord): los shaders que ya viste se vuelven a
  armar en segundo plano al empezar, de a uno, como en el port de MHGU, así que los tirones cortos en cada efecto nuevo
  deberían ser mucho menos frecuentes después de tu primera sesión. Un PowerVR BXM que se cerraba en cada arranque
  debería arrancar ahora. Los PowerVR más viejos que mostraban pantalla negra con sonido ahora dibujan el 3D a lo máximo
  que permite su GPU: la fila de la resolución 3D lo dice, por ejemplo "100 % (5x) · 4x".
- **Sesiones largas en un PowerVR BXM** ([#31](https://github.com/khorne-hub/4u-native/issues/31)): `log.txt` ahora dice
  qué es lo que crece hasta que el juego se cierra, y cerca del límite del sistema la app pasa a una forma más lenta pero
  más segura de mostrar las pantallas. Puede ayudar; un registro de una sesión larga nos dirá más.
- **HUD en los bordes de la pantalla:** los medidores de las armas debajo del filo (los frascos de la Charge Blade, los
  de las otras armas), el Frenesí y la munición de una ballesta ahora se mueven con el resto del HUD (Discord).
- **Más detalle en el registro:** después de un cierre dentro del driver de la GPU, el nombre del hilo y las líneas de
  error del propio sistema (Discord); la línea de rendimiento de cada minuto ahora trae el estado de temperatura del
  teléfono y la velocidad del procesador, para saber si un bajón viene del calor del teléfono
  ([#50](https://github.com/khorne-hub/4u-native/issues/50), Discord); y una imagen equivocada durante un solo cuadro
  deja líneas en el registro (Discord).
- **La herramienta de la librería en la PC:** su ventana ya no se cierra enseguida. Avisa si la descarga no se
  descomprimió antes, y siempre espera para que puedas leer sus últimas líneas (Discord).
- **Títulos de las ventanas para elegir:** el título entra entero en una pantalla 4:3, con su rombo y su **?**: se achica
  si hace falta, en vez de cortarse.
- **El engranaje y los menús del juego:** con el modo de pantalla única (o el de un mod de código) en una pantalla 4:3, el
  engranaje de opciones espera en la esquina de abajo a la izquierda mientras un menú del juego está en la pantalla de
  arriba, en vez de taparlo; un engranaje que moviste queda donde lo pusiste.
- **Los botones táctiles y los menús del juego:** con el modo de pantalla única (o el de un mod de código), mientras un menú
  del juego está en la pantalla de arriba (la caja de objetos, la bolsa, una tienda), un botón táctil con la mitad de su
  imagen o más encima del menú se dibuja tenue para que el menú se lea entero; en un teléfono 20:9 los botones de la derecha tapaban la última
  columna de la caja de objetos. Siguen funcionando igual y nada se mueve.
- **Importar partida desde la PC** (Discord): una partida de Azahar, Citra, Checkpoint o JKSM entra desde la carpeta que
  elijas: la carpeta entera del emulador, su `sdmc`, la carpeta `data` o la del título de la partida, o la carpeta de la
  herramienta de copias. Una partida europea iba a donde el juego de EE. UU. nunca mira, así que tu cazador no aparecía:
  ahora una partida de la otra región (o del MH4G japonés) se rechaza con un mensaje que nombra los dos juegos, igual que
  un archivo de cazador cortado o uno sin su archivo `system` cuando todavía no tienes partida. Las tarjetas de gremio,
  misiones de evento y Felyne que están junto a la partida de un emulador también vienen (antes nunca venían). Después
  del reinicio un aviso dice qué se trajo, y **Compartir registro** incluye la última importación. No pudimos reproducir
  la pantalla negra después de importar: si te sigue pasando, mándanos el registro.
- **README:** la instalación en resumen, la herramienta desde el **Source code (zip)** de la misma versión que tu APK,
  cómo pasarte desde una copia de otra página sin perder tu partida, más filas en *Si algo sale mal*, el botón Fijar
  objetivo, y para la 1.0: crear el código del juego en el teléfono, el modo de pantalla única, jugar en línea paso a paso,
  chat y amigos, juego cruzado, importar las misiones de evento, tener tu partida en la nube con una app de
  sincronización, los packs HD y los lugares del menú nuevo.

## 0.1.9.2

Tu librería de código sigue sirviendo: no hace falta crearla de nuevo.

- **Botones combinados en un mando:** cada botón combinado puede ir en un botón del mando (L3, R3, SELECT, Guide, Misc,
  las paletas o el panel táctil): engranaje, **Controles**, **Mando**, **Botones combinados**, una fila que se ve con un
  mando conectado. Una sugerencia de un jugador en Discord.
- **Más detalle en el registro para teléfonos lentos** ([#43](https://github.com/khorne-hub/4u-native/issues/43)):
  `log.txt` ahora dice cuánto lleva armada la lista de shaders del juego mientras se arma, y cuánto tardó, y tiene una
  línea de rendimiento por minuto.
- **El mod de una sola pantalla** está hecho para el juego de América en inglés: con el juego en otro idioma sus menús se
  superponen ([#30](https://github.com/khorne-hub/4u-native/issues/30)). Ahora el README lo dice.

## 0.1.9.1

Tu librería de código sigue sirviendo: no hace falta crearla de nuevo.

- **Arreglado el parpadeo con la pantalla táctil oculta** (cuadros claros, oscuros o rotos cada pocos segundos que
  paraban al mostrar la pantalla táctil) en GPU con drivers gráficos más viejos, como Mali-G71 y G72, y en PowerVR: ahí la
  pantalla de arriba llega ahora siempre de la misma forma a la pantalla del teléfono. Las GPU Mali y Adreno con drivers
  recientes no cambian.

## 0.1.9

Tu librería de código sigue sirviendo: no hace falta crearla de nuevo.

- **Correr con la palanca:** empújala más allá de su borde para correr, y levanta el pulgar sobre el círculo del candado,
  encima de ella, para seguir corriendo; volver a tocar la palanca o el círculo lo corta. El círculo se mueve y cambia de
  tamaño en el editor del diseño. Desactivado, un botón **Correr** corre mientras lo mantienes.
- **Arcos y ballestas:** un toque en apuntar deja la mira puesta hasta el próximo toque; mantén y desliza para apuntar.
  **Apuntar con giroscopio** (desactivado por defecto), con su sensibilidad, gira la mira al girar el teléfono.
- **Pad de menú:** en los menús del juego aparece un pad chico bajo el pulgar derecho, con L y R encima, en lugar de la
  cruceta chica de los menús. Las recompensas de la misión y la Caja de recompensas cuentan como menú, y la cruceta de
  cacería se aparta mientras se ve el pad.
- **Fijar objetivo:** se fue el candado junto al ícono de objetivo, que no respondía; un aro dorado alrededor del ícono
  muestra que está fijado.
- **Pantalla negra con la música del menú en algunas GPU Mali viejas:** cuando los gráficos del teléfono rechazan la forma
  habitual de mostrar la imagen, la app ahora pasa sola a otra, y `log.txt` dice más sobre los gráficos del teléfono.
- **Imagen:** las escenas vuelven a verse nítidas; se detectan las texturas que el juego reescribe (se acabaron la ropa con
  el dibujo anterior al crear el personaje y el suelo o el cielo equivocados); las sesiones largas usan menos memoria para
  las texturas, así siguen cargando en teléfonos de 3-4 GB; 30 fps estables en teléfonos que pasan la pantalla a 30 Hz.
- **Otras GPU:** los drivers Vulkan 1.1 más viejos reciben shaders que aceptan; en Adreno el mínimo de 3x es solo para
  drivers anteriores a 1.1.128, y **Corrección de Adreno** lo apaga; los teléfonos de la clase Adreno 610 empiezan con los
  ajustes livianos; PowerVR y otras GPU ya no dibujan sobre una imagen que todavía está en pantalla.
- **Carpeta de partidas** (Opciones del menú): guarda tu partida en una carpeta que una app de sincronización comparta con
  tus otros dispositivos. El MHGU para Android puede compartir la misma carpeta.
- **Importar partida** tiene **Elegir carpeta** y encuentra solo las carpetas de los emuladores. Una importación nunca borra
  tu partida anterior antes de que la nueva esté en su lugar.
- **Comer y beber más rápido:** dos trucos, apagados, como en el port del MHGU. **Comodidades** en Trucos, según los mods
  QOL de jM5557: mantener A para recolectar, moverse corta la pose, B cancela el afilado, recompensas sin tiempo.
- **Jugar con otros:** viene puesto un lobby de la comunidad, los lobbies que escribes se recuerdan (la × quita uno), y la
  primera vez en línea un cartel dice a qué servidor se conecta.
- **Jugando solo,** el juego espera mientras las opciones o el editor del diseño están abiertos.
- **Botones de GitHub y Discord** junto a Guardar: abren la página del proyecto y el Discord de la comunidad (preguntan
  antes).
- **Avisos:** como mucho una vez al día la app busca una versión nueva (**Buscar actualizaciones** lo apaga), y una copia
  que no es la oficial lo avisa.
- Además: correr y tocar la cámara en una aldea ya no abre el mapa del mundo del mod de una sola pantalla (L y R juntos sí);
  un pulgar que cae en la palanca toma la palanca; en la pantalla de error la vuelta a los mods del último inicio funciona
  con un toque, también para un mod de código que la app no puede cargar; el panel del editor se mueve y se achica; opción
  de 240 fps.

## 0.1.8

- **Arreglada una franja de colores equivocados en el borde de arriba** con resoluciones 3D bajas (modo gama baja, 50 %): el
  juego nunca dibuja la primera fila de la pantalla de arriba, que a 1x se veía como una línea entera.

## 0.1.7

- **Arreglada la pantalla negra con "No game folder at /game"** en teléfonos donde Android no le da a la app una carpeta
  externa (por ejemplo un Redmi Note 8): el juego ahora lee su configuración de la misma carpeta donde la guarda la pantalla
  de la carpeta del juego.
- **GPU Adreno con driver Vulkan 1.1** (por ejemplo la Adreno 630 del Galaxy S9): su driver dibuja mal la imagen 3D por
  debajo de 720 líneas (franjas de colores), así que ahí la resolución 3D es de 3x como mínimo.

## 0.1.6

- **GPU PowerVR:** con la 0.1.4 se podía jugar, pero al volver a entrar se cerraba otra vez: el compilador de shaders de la GPU
  también fallaba cuando la app armaba sus shaders guardados en un segundo hilo. Ahora cada shader se arma en un solo hilo la
  primera vez que hace falta.

## 0.1.5

- **Arreglada la página de botones combinados** (engranaje, Controles, botones en pantalla, Botón 1-6): no mostraba nada.

## 0.1.4

- **GPU PowerVR** (por ejemplo la GE8300 del Helio A22): el juego se cerraba a los pocos segundos de iniciar, cuando el
  propio compilador de shaders de la GPU fallaba con el shader más grande de la app. Ahí ya no se arma ese shader; cada
  shader se arma la primera vez que hace falta, así que un efecto nuevo puede dar un tirón una vez. Sin probar en un
  teléfono PowerVR: cuéntanos cómo te va.

## 0.1.3

- **Arreglado el cierre al iniciar que la 0.1.2 todavía tenía en algunos teléfonos con driver gráfico Vulkan 1.1** (por
  ejemplo teléfonos con Mali-G57 y chip Unisoc). Tu librería de código sigue sirviendo.

## 0.1.2

- **Arreglado el cierre justo después de iniciar en teléfonos cuyo driver gráfico es Vulkan 1.1** (por ejemplo algunos
  Adreno 730): la 0.1.1 pasaba la pantalla negra y se cerraba en el primer cuadro. Tu librería de código sigue sirviendo.
- **Arreglado el cierre del juego en el primer inicio en algunos teléfonos con Android 14** (por ejemplo un Moto g55): no
  podía crear sus carpetas de partidas.

## 0.1.1

- **Arreglada la pantalla negra al iniciar en teléfonos con Adreno 7xx** (por ejemplo el Adreno 730 del Snapdragon 8+ Gen
  1): su driver gráfico se rechazaba al iniciar. Tu librería de código sigue sirviendo: no hace falta armarla de nuevo.

## 0.1.0

La primera versión pública: Monster Hunter 4 Ultimate (América, 1.3.0) recompilado a código nativo para Android, con tu
propia copia del juego.

- **Nativo, no emulado:** el código del juego se recompila en tu PC a una librería para tu teléfono, y corre sobre un
  runtime propio, con los gráficos de la 3DS dibujados con Vulkan.
- **La resolución de tu teléfono** por defecto, pantalla ancha igual a tu pantalla (también las consolas 4:3, sin barras) y
  un límite de cuadros por segundo de 30 a 240, con ritmo estable opcional.
- **Modo gama baja** y **modo Tostadora** para teléfonos débiles y cacerías largas con una sola carga; se activa solo la
  primera vez en un teléfono débil.
- **Controles táctiles pensados para teléfonos:** fijar objetivo con el candado (la cámara sigue al monstruo) y el botón de objetivo para cambiar de monstruo, la barra de objetos con toques y deslizamientos sin
  cruceta, un diseño propio para arcos y ballestas, botones combinados, y un editor del diseño que también mueve el HUD del
  juego.
- **Mandos y teclados:** la palanca derecha como cámara, mapeo de botones por mando, START+SELECT para las opciones, el
  teclado completo.
- **Acciones del pueblo sin la pantalla táctil:** Tarjetas del Gremio, Gestos, Amigos y Cazadores de alquiler desde una
  lista (un botón táctil en el pueblo, SELECT en un mando); lo que abren se ve grande en el centro, con un cursor para el
  mando.
- **Menús con tu propio diseño:** los controles no cambian al abrir un menú. Una cruceta chica aparece solo mientras un
  menú la necesita (cajas de objetos, tiendas, el tablón de misiones, el menú de START…), y tus botones bajan a la mitad
  para que el menú se lea. Las dos cosas se pueden mover, cambiar de tamaño o apagar.
- **Flechas de la barra de objetos:** un toque mueve un objeto y mantenerla repite; siguen a la barra donde la pongas.
  Deslizar sigue funcionando.
- **Los toques rápidos siempre cuentan:** un toque en la pantalla táctil se sostiene hasta que el juego lo ve.
- **Créditos y licencias:** los créditos al abrir la app, un agradecimiento que aparece una sola vez, y una página con los
  créditos, el aviso legal y todas las licencias.
- **El mod de una sola pantalla** funciona (se descarga de su autor; va dentro de tu librería).
- Mods con orden de carga y conflictos, trucos en el formato de Azahar, salas de red y salas en la misma Wi-Fi, importar y
  exportar partidas, tu propio driver de GPU en Adreno, contador de FPS con la temperatura, y una pantalla de error que
  comparte el registro.
- Android 9 o superior, de 64 bits.

### Conocido

- El mapa grande de zonas del mod de una sola pantalla (aparece al tomar el Mapa de la caja de suministros) todavía no se
  puede mover.
- En el pueblo, SELECT abre las acciones del pueblo en lugar de "Personalizar paneles" del juego; para recuperarlo, pon
  "Botón de acciones del pueblo" en otro botón o apagado, en Controles.
- Con un perfil de juego de ahorro de energía, algunos teléfonos sostienen unos 105-110 FPS en lugar de 120 en el campo; a
  60 es estable.

### Descargas oficiales

MH4U para Android es gratis. Solo los links que da khorne-hub son oficiales: si la bajaste de otro lado, pagaste por ella o
vino con publicidad, no es la oficial. Compara el SHA-256 del APK con el archivo `.sha256` de la versión.
