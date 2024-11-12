# Artemis PS3
An Open Source Playstation 3 Hacking System created under the [Project Artemis initiative](http://www.gamehacking.org/artemis)

[![Downloads][img_downloads]][artemis_downloads] [![Release][img_latest]][artemis_latest] [![License][img_license]][app_license]
[![Build package](https://github.com/bucanero/ArtemisPS3/actions/workflows/build.yml/badge.svg)](https://github.com/bucanero/ArtemisPS3/actions/workflows/build.yml)

Notes
-----------
This is a fork of Dnawrkshp's [ArtemisPS3](https://github.com/Dnawrkshp/ArtemisPS3), that includes the following network features:

 - **Online Cheat Database:** access to the latest cheats for over +2000 games online.
 - **Local Database Update:** download and update your local cheat database with the latest codes.

Download
----------
Get a `.pkg` installer for the [latest version here][artemis_latest].

How To Use
----------
Please refer to the [Installation](./docs/INSTALLATION.md) file for installation and use instructions.

Submitting cheat codes
----------

To share new games and cheat codes with the community:

 1. Fork the repository and add your changes to [this folder](https://github.com/bucanero/ArtemisPS3/tree/master/docs/codes).
 2. Submit a pull request so the updated cheats are available to every Artemis user.

Screenshots
----------
![main](./docs/screenshots/main.png "Main Menu")
![games](./docs/screenshots/games.png "Games List")
![cheats](./docs/screenshots/cheats.png "Cheats List")
![options](./docs/screenshots/options.png "Options")
![about](./docs/screenshots/about.png "About")

Status
-------

- [X] Load artemis_ps3.sprx into VSH
- [X] Access codes from an online database
- [X] Access user codes
- [ ] Allow for users to enter their own codes from Artemis
- [X] Sorts codes and games alphabetically


Credits
-------

    Lazy Bastard    -   Project Founder
    Berion          -   GUI Graphic Designer
    Dnawrkshp       -   Creator of ArtemisPS3-GUI and ArtemisPS3-PRX
    NzV             -   PS3MAPI (on which Artemis is dependant upon)
    PS2Dragon       -   Artemis Logo
    Bucanero        -   Network code
	

Dependancies
------------

    ArtemisPS3-GUI    -   PSL1GHT SDK (github www.github.com/HACKERCHANNEL/)
    ArtemisPS3-PRX    -   CELL SDK

Compiling
-------

This has been built using a makefile and my [VS Integration tools](https://github.com/Dnawrkshp/PS3-VS-Integration).
Ideally, you'd use Visual Studio to run and compile as normal. However the command line will still operate functionally.

For the following, only selfs can be packaged and run.
--
    Visual Studio
      Build           -   Set mode to Debug. Build as normal (F6)
      Run             -   Set mode to Debug. Run as normal (F5)
      Package         -   Set mode to Release. Build as normal (F6)
    Command Line
      Build           -   make
      Run             -   make run
      Package         -   make pkg
	  

[artemis_downloads]: https://github.com/bucanero/ArtemisPS3/releases
[artemis_latest]: https://github.com/bucanero/ArtemisPS3/releases/latest
[img_downloads]: https://img.shields.io/github/downloads/bucanero/ArtemisPS3/total.svg?maxAge=3600
[img_latest]: https://img.shields.io/github/release/bucanero/ArtemisPS3.svg?maxAge=3600
[app_license]: https://github.com/bucanero/ArtemisPS3/blob/master/LICENSE
[img_license]: https://img.shields.io/github/license/bucanero/ArtemisPS3.svg?maxAge=2592000
