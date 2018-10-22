# V2.1
param(
    [ValidateLength(4,4)][ValidatePattern("[C][O][M]\d{1}")][STRING]$ComPort,  # specify Com port - ex: COM1, COM3, etc...
    $mameProcessName = "mame64",
    $mameResolutionFile = "$PSScriptRoot\mame_res_info.json",
    $messageConfigFile  = "$PSScriptRoot\crt_config.json"
    )

#  UART - Get last Com port - can be used for auto-detection
#============================================================
Function Get-LastComPOrt(){
    $LastCom = Get-WMIObject Win32_SerialPort | Select-Object DeviceID,Description | Select-Object -Last 1
    return ($LastCom.DeviceID)
 }
 
#  UART - Send DATA to Com UART port
#============================================================
Function Send-I2Cmessage($text){
    if($ComPort -eq ""){$ComPort = Get-LastComPOrt}
    Write-host "COM port: " -NoNewline;Write-Host $ComPort -f Yellow
    
    write-host "Data to send to $ComPort`: " -NoNewline;Write-host $text -f Cyan
    $port = new-Object System.IO.Ports.SerialPort $ComPort,57600,None,8,one
    $port.Open()
    $port.WriteLine( "$text`r`n" )
    [console]::beep(500,300)
    #start-sleep -m 200
    $result = $port.ReadExisting()
    $result | %{write-host $_}
    $port.Close()
    Write-host "Press Ctrl+C to stop." -f gray
}

#  RESOLUTION - Get current System or Mame Game resolution and running Mame game, if any.
#============================================================
Function Get-Resolution(){
    $res = get-wmiobject win32_videocontroller  | Select VideoProcessor,VideoModeDescription,CurrentBitsPerPixel,Driver*,CurrentRefreshRate
    [STRING]$reso = [STRING]($res.VideoModeDescription -split(' '))[0] + "x" + [STRING]($res.VideoModeDescription -split(' '))[2] + '@' + [STRING]($res.CurrentRefreshRate)
    write-host "Current resolution: $reso"

    # Catch Mame running game Name
    Write-host "Looking for '$mameProcessName' process: " -NoNewline
    $Game = (Get-Process -Name $mameProcessName -ErrorAction SilentlyContinue).MainWindowTitle
    if($Game){
        $Game = $Game.Split('[')[1] -replace(']','')
        Write-host " found! Running game: $Game"
        if($mameResolution.$Game){
            Write-host "Mame game resolution: $($mameResolution.$Game)"
            return $mameResolution.$Game,$Game
        }
    }else{write-host "not found."}
    return $reso,$false
}

#  RESOLUTION - Identify resolution settings, if any, and ask to send to Com port
#============================================================
Function Set-CrtTuning([switch]$ReportOnly){
    
    #Write-host "$((Get-Process -Name ).MainWindowTitle)" -f Cyan
    
    $infoRes = get-resolution
    $res = $infoRes[0]
    $mameGame = $infoRes[1]
    Write-host "Set-CrtTuning -res $res -mameGame $mameGame"
    $Message = Get-I2CmessageToSend -res $res -mameGame $mameGame
    if($ReportOnly.IsPresent){Write-host "[Simulation mode, message not send]";return}

    if($Message){Send-I2Cmessage $Message}
    else{write-host "No configuration found,Send STOP";send-I2Cmessage ']'} # send STOP to free I2C TV bus
}

#  EVENTS - Stop event capture - could prevent memory leak
#============================================================
Function Stop-Monitors(){Get-EventSubscriber | Unregister-Event}

#  EVENTS - Start event trapping - ChangeResolution & config file changed
#============================================================
Function Start-Monitors(){
    # 1 - Trap resolution change event
    Write-host "Set Resolution change event trap"
    $sysevent = [microsoft.win32.systemevents]
    $a = Register-ObjectEvent -InputObject $sysevent `
        -EventName "DisplaySettingsChanged" `
        -Action {Write-host "Resolution change detected!" -ForegroundColor Cyan;Set-CrtTuning}

    # 2 - Monitor CRT config file change
    $pathM = (get-item $mameResolutionFile).Directory.FullName
    $FileM = (get-item $mameResolutionFile).Name
    
    Write-host "Set Configuration file change event trap"
    $fileWatcher = New-Object IO.FileSystemWatcher  "$PSScriptRoot\", "crt_config.json" -Property @{IncludeSubdirectories = $false;NotifyFilter = [IO.NotifyFilters]'FileName', 'LastWrite'}
    $evConfig = Register-ObjectEvent $fileWatcher Changed -SourceIdentifier FileChanged -Action { 
    $name = $Event.SourceEventArgs.Name 
    $changeType = $Event.SourceEventArgs.ChangeType
    $timeStamp = $Event.TimeGenerated
    if($mark -ine "$name-$timeStamp"){  # Mark weird things to prevent double trap
        Write-Host "The file '$name' was $changeType at $timeStamp" -ForegroundColor white
        $mark = "$name-$timeStamp"
        Get-Configuration
        Remove-Event FileChanged}
    }
}


#  RESOLUTION - Cross resolution/Mame game to Configuration to get I2C message, if any
#============================================================
Function Get-I2CmessageToSend($res,$mameGame){
    if($mameGame){
        Write-host "Mame is running '$mameGame'"
        #$res = get-MameGameRes $mameGame           # Get game native resolution
        $i2cMessage = $config.mameGame[$mameGame]  # Look for game in config file
        if($i2cMessage){
            Write-host "Specific Mame game config found: $i2cMessage" -ForegroundColor Cyan
            return $i2cMessage
        }
        ### Look for Mame resolution config
        $GenEntry = $config.mameResolution.Keys | ?{$res -match $_} | Select-Object -First 1
        if($GenEntry){
            Write-host "Mame resolution config found: $GenEntry"
            return $config.mameResolution[$GenEntry]
        }
    }
    $i2cMessage = $config.resolution.$res
    if($i2cMessage){
        Write-host "System resolution configuration found: $i2cMessage"
        return $i2cMessage
    }
    Write-host "No configuration to apply."
}

Function Get-FromJson{
    #https://stackoverflow.com/questions/40495248/powershell-hashtable-from-json
    #[CmdletBinding]
    param(
        [Parameter(Mandatory=$true, Position=1)]
        [string]$Path
    )
    function Get-Value {
        param( $value )

        $result = $null
        if ( $value -is [System.Management.Automation.PSCustomObject] )
        {
            Write-Verbose "Get-Value: value is PSCustomObject"
            $result = @{}
            $value.psobject.properties | ForEach-Object { 
                $result[$_.Name] = Get-Value -value $_.Value 
            }
        }
        elseif ($value -is [System.Object[]])
        {
            $list = New-Object System.Collections.ArrayList
            Write-Verbose "Get-Value: value is Array"
            $value | ForEach-Object {
                $list.Add((Get-Value -value $_)) | Out-Null
            }
            $result = $list
        }
        else
        {
            Write-Verbose "Get-Value: value is type: $($value.GetType())"
            $result = $value
        }
        return $result
    }
    if (Test-Path $Path)
    {
        $json = Get-Content $Path -Raw
    }
    else
    {
        $json = '{}'
    }

    $hashtable = Get-Value -value (ConvertFrom-Json $json)

    return $hashtable
}

Function Get-Configuration(){
    try{$global:Config = Get-FromJson $messageConfigFile}
    catch{Write-host "Unable to load configuration file: $_" -f Yellow;return}
    Write-host "Configuration file loaded!"
}

#============================================================
#     MAIN
#============================================================
Stop-Monitors

# Read config files
try{
    $mameResolution =  Get-Content $mameResolutionFile | ConvertFrom-Json
    $config =          Get-FromJson "$messageConfigFile"
} catch{Write-host "Error reading required files: $_"}

Start-Monitors

Write-host "Press Ctrl+C to stop." -f gray
$host.ui.RawUI.WindowTitle = "USB4CRT - Waiting events - Ctrl+C to quit"
Wait-Event

Stop-MonitorResolutionChange