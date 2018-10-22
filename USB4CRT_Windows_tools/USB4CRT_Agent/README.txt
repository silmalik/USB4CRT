USB4CRT Agent

Tool to send I2C commands to USB4CRT device at resolution change.

The "USB4CRT_Agent.ps1" script traps system resolution change, identify IC2 commands upon context.
If any, will send I2C commands on the USB4CRT device virtual COM port.

The script catch if Mame is running and which game is emulated (based on process name and main Window title).

Some I2C commands can be defined upon:
 - Mame specific running game
 - Running Mame game video mode or video mode pattern (ex: 256@55 for 384x256@55.0000001 games, etc.).
 - System resolution

System resolution have to be differs from 


Configuration files:

  crt_config.json:
    Contains pattern and I2C commands
    Can modified while USB4CRT agent is running, if changed is done, the script will reload the file

    Sections:
    - "mameGame": for specific mame game, just in case, not recommended
    - "mameResolution": can be filled with full video mode description or pattern
    - "systemResolution": For "out of Mame" usage

  mame_res_info.json:
    Contains Mame games video modes, based on mame.xml famous file.
    Provided "mame_res_info.json" is based on mame200.xml
    If required ".\addons\USB4CRT_Mame_XML_to_Json.ps1" is provided to generate a new .json from xml (warning: heavy process!).

Usage:

For testing:
	Edit USB4CRT_Agent.ps1 with Powershell_ISE

	- have a look the default "ComPort" and "mameProcessName" default values, adjust if required.
	  ComPort: by default, the script will take the last one on your system, should be ok on most systems
	  mameProcessName: must be same as your Mame/GroovyMame process, without extension.

	- Execute from the Powershell_ISE


To launch in a hidden way, create a shortcut with a similar command line (run minimized):

	C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -MTA . C:\USB4CRT\USB4CRT_Agent.ps1
		
	--> Please, take care of the "." and the "MTA" options, they are required.
	--> If required, Add options "-mameProcessName myMameBinary -ComPort COMx":

	C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe -MTA . C:\USB4CRT\USB4CRT_Agent.ps1"-mameProcessName groovymame -ComPport COM5



