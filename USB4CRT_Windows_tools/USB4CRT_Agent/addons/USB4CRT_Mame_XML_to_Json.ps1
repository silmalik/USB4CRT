# v1.0
Param(
    $mameXMLfilePath = ".\mame0201.xml",
    $mameResInformationFilePath = ".\mame_res_info.json"
    )

if(-not $xml){
    Write-host "Loading XML file '$mameXMLfilePath', please wait..." -NoNewline
    $delay = Measure-command{[xml]$xml= Get-Content $mameXMLfilePath}
    Write-host " Done in $([INT]$delay.TotalMinutes)mn!"}
else{write-host "XML already loaded"}

$info=@{}
$nb = $xml.mame.machine.count
$current = 0

ForEach($m in $xml.mame.machine){
    $current++
    Write-Progress -Activity "Extracting resolution information." -Status "Please wait..." -PercentComplete ($current / $nb * 100)
    
    if(($m.display.width).count -eq 1) {
        $info.Add($m.name,"$($m.display.width),$($m.display.height ),$($m.display.refresh)")
    }
    if(($m.display.width).count -gt 1){
        $info.Add($m.name,"$($m.display.width[0]),$($m.display.height[0]),$($m.display.refresh[0])")
    }
}
Write-host "Export resolution to Json file '$mameResInformationFilePath'" -NoNewline
$info | convertto-json | out-file $mameResInformationFilePath -Force
Write-host " - Done."

