# Changelog

CM 01/02 Bosman Edition uses semantic versioning. A patch release (0.1.1) fixes bugs. A minor release (0.2.0) adds features. A major release (1.0.0 and later) can break saved games or settings. Releases below 1.0.0 are test releases.

## Next release

- Setup Game: two new buttons sit at the top. The left one shows the season and its database, and a click opens Choose Season. The right one shows whether the rules change, and a click switches it. Hall Of Fame, Game Credits and Web Sites are on a new Extras screen.
- Choose Season: each season has one database, so the version choice is gone. The changing rules switch is on this screen now, under the season details.
- The start screens have the title "Championship Manager: Bosman Edition".
- Bosman Edition Options: the Screen tab is now called Graphics.
- Attribute groups: a goalkeeper's profile shows One On Ones and Consistency in the Goalkeeping group. Both come from the game's own player data, which the original does not show. Attributes that your scouts do not know show as " -", as the others do.
- Bosman Edition Options: the Save folder row has a Change button that opens your computer's folder picker. The game uses the new folder from the next start. Saved games already in the old folder stay there.
- Team views: Best XI and Squad depth are one screen. The pitch lies on its side with a card for each of the 11 places: the Best XI player, then the next two for that position. Stars show how well each player fits the place, and players who cannot play the next match are grey.
- HD mode is gone. On a Mac screen it showed bars around the picture and small text, while widescreen fills a large screen with sharp, bigger pixels. A `display = hd` line in `settings.txt` now gives widescreen.
- Display report: if the picture looks wrong, press Ctrl+Shift+D (Cmd+Shift+D on a Mac). The game saves `display-report.txt` in the save folder, puts the same text on the clipboard and shows where the file is. The report lists your system, your screens and their scale, the window size and how the game scales its picture, and the last 10 changes of the window. The game also writes the file at each start and quit. Paths in it show `<user>` in place of your account name.
- Windows: text stays sharp with display scaling at 125 % or 150 %. Before, Windows stretched the whole window and blurred it. A new window opens at the size of the picture, as at 100 %.
- The picture fills the window at any size, larger or smaller, and keeps its shape. Black bars show only where the window has a different shape from the picture. With `sharp_picture` on, letters keep an even shape at every size. With it off, the picture is smoother and softer.
- The window cannot get smaller than the picture: 800x600 in classic mode, 1280x800 in widescreen. On a smaller screen the limit is the largest window that fits.
- Classic mode in full screen (F11, or Cmd+F on a Mac) changes the screen to 800x600, as the original does, when the screen has that mode. Macs have no 800x600 mode, so there the picture fills the screen.
- On a screen less than 800 pixels high, widescreen shows a message once at start, and the Display mode row of Bosman Edition Options says that classic mode suits the screen better.
- The Linux download is now `CM0102-Bosman-Edition-<version>-x86_64.AppImage`, and it has the icon file that AppImage tools look for.

## 0.5.0 (pre-release)

An options box in the game, positions on the tactics screen, foreign players in colour, and fixes for rare faults of the original. Each new option has a line in `settings.txt` in the save folder.

- Bosman Edition Options: a box with two tabs, Screen and Game, that shows every option of the Bosman Edition. It opens from Setup Game and from Game Options in the side bar, after Game Settings. A change saves to `settings.txt` at once and keeps your own lines and comments. Display mode, All sound and Save folder take effect at the next start. Save folder is shown, and you change it in `settings.txt`.
- The tactics screen shows each player's position in the Condition view, before and during a match, in classic, widescreen and HD (`match_columns`).
- Foreign players in colour (`foreign_colours`): on the squad and tactics screens, the names of players who count against the foreign player limit of the club's next league or cup match are light violet. It works for every club. The game's "Fgn" status stays.
- Full player search (`full_player_search`, off by default): the player search lists every player, also players your manager does not know. With it on, a search changes later days, as opening a screen does.
- Fixes for rare faults of the original game: a news item with no text, some transfer offers, squad updates, staff records after a part-exchange deal, the competition table and the World Cup draw. The original crashed on these. The port went on with wrong data. Now it skips the bad item.
- The game no longer uses a full processor core while it waits for input (about 1% in place of 100%). Matches and holidays play the same.
- The Database button shows 3.9.68, 3.9.60 and 3.9.65 first, then your databases from the oldest season to the newest, each with its season and folder name, for example "2025/26 (May 2026)". In classic mode a long folder name shows the season alone. A folder whose name has no month and year comes last.
- The intro shows the CM 01/02 logo with every database, and a database in `Databases` no longer replaces the pictures of the intro.
- A new Bosman Edition logo, on the site and in the intro.

## 0.4.0 (pre-release)

New screens and an HD layout. Each new option has a line in `settings.txt` in the save folder. No feature changes the game's decisions while you make the same clicks.

- Team views on every squad screen, from the Assistant menu (`squad_views`): your best XI on a pitch in any of the game's formations, with the subs and other players beside it, and a squad depth table with the best players for each position. The numbers are the assistant's score for each position, from the attributes the profile shows. Players who cannot play the next match are left out of the eleven and shown in grey in the table.
- A player comparison with a polygon of eight strengths, on the original's comparison screen and on the Compare tab. Two goalkeepers get goalkeeper strengths.
- Attribute groups on the player profile (`attribute_groups`): Technical, Mental and Physical, with Goalkeeping first for goalkeepers.
- Print to PDF: Print To shows PDF in place of Printer, and each print saves a PDF in the `Prints` folder of the save folder. File still saves a text file, and Both saves both.
- An HD layout at 1920 × 1080 (`display = hd`). A new window shrinks to fit the screen.
- The CM 01/02 Update Team's databases from 2003 to 2026: copy a database into a folder in `Databases` in the save folder, and choose it with the Database button. The game still starts in July 2001.
- Message boxes are centred in widescreen and HD.
- The box "CD Not Required" no longer shows after a new game or a load.
- A clickable name no longer shows its underline under a box that covers it.

## 0.3.1 (pre-release)

A fix for the Assistant menu on the squad screen.

- Pick best team, Pick most match-fit team and Pick under-21 team now score each player by the key attributes of the position, as the profile shows them. For example, a goalkeeper is scored on Handling, Reflexes, Agility, Positioning and Jumping. The score also counts how well the player fits the position.
- The assistant finds the best eleven as a whole. In 0.3.0 it filled one position at a time, so a natural right back could sit on the bench while a centre back played at right back.
- A player plays out of position only when no free player suits that position.

## 0.3.0 (pre-release)

New features and a Linux version. Each option has a line in `settings.txt` in the save folder. No feature changes a match result or a decision of the game, except the team that you or your assistant pick.

- Linux: one AppImage for 64-bit Linux. It also runs on the Steam Deck in desktop mode.
- A Database choice on the Setup Game screen for each new game: the original 2001/02 data (3.9.60), the 2001/02 December update (3.9.65) or the 2002/03 data (3.9.68, the default). The `database` line holds the choice.
- An Assistant menu on the squad screen picks the best team, the most match-fit team or an under-21 team. It weighs each player's current ability and how well the player fits each position.
- Holiday until next match: an item below Go on Holiday ends the holiday on the day before your club's next match.
- A Fastest commentary speed, twice as fast as Very Fast. As in the original game, the commentary speed can change how a match plays out.
- Save Game suggests a name with your club and the game date, for example "Arsenal 2001-07-20".
- The game keeps the last three copies of each save in a `backups` folder next to your saves (`save_backups`).
- The window opens at the size and place it had when you quit, in full screen if it was (`remember_window`).
- Condition and morale have colours: red below 75%, amber from 75 to 89% and green from 90% (`condition_colours`).
- Windows: Ctrl+[ is Back and F11 switches full screen.
- Fixes: the underline in the news covers the whole name, and the "Replace this file?" text is centred in widescreen.

Thanks to u/gundenaf on Reddit, who asked for the Linux version and the database choice.

## 0.2.0 (pre-release)

Comfort features and sound. Each feature has a line in `settings.txt` in the save folder, and `off` turns it off. No feature changes a match result or a decision of the game.

- Sound: the game's own sounds play. The game is silent while its window is not in front. `sound = off` mutes it.
- The space bar presses Continue Game.
- Back: Cmd+[ on a Mac, or the back button of a mouse. Escape closes a popup. Cmd+F switches full screen on and off.
- The picture scales in whole pixels, so text stays sharp.
- `save_folder` keeps saved games in another folder, for example in iCloud Drive.
- Player and club names show a hand and an underline under the mouse. A click opens the profile.
- Attribute numbers have colours by value: 1 to 5 red, 6 to 10 amber, 11 to 15 light green, 16 to 20 bright green.
- In a match, widescreen only: the tactics screen shows each player's positions and condition beside the match rating, and the Player Ratings view shows the condition of both teams.
- Days pass faster while you are on holiday.
- Widescreen fixes: longer texts fit, and the name buttons in the news land on the names.

## 0.1.0 (pre-release)

The first test release.

- Championship Manager 01/02 at version 3.9.68: the 2008 free release with the last official patch. No fan patches or fan databases.
- Mac: one app for Apple silicon and Intel, macOS 11 or later.
- Windows: an installer for 64-bit Windows 10 and 11, with Start menu and desktop shortcuts.
- Widescreen at 1280 × 800 by default. The window can be resized and keeps the shape of the picture.
- The old 800 × 600 layout: open `settings.txt` in the save folder and change the line to `display = classic`.
- Saves and settings live in `~/Library/Application Support/cm0102` on a Mac and in `%APPDATA%\cm0102` on Windows.

Not in this release: sound, network play and printing.
