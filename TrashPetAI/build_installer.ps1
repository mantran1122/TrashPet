param(
    [string]$StagingDir = "$PSScriptRoot\installer_staging",
    [string]$OutputExe  = "$PSScriptRoot\TrashPetAI_Setup.exe"
)

$sedPath = "$PSScriptRoot\trashpetai_installer.sed"

# Liệt kê toàn bộ file trong staging (đường dẫn tương đối, giữ cấu trúc thư mục con)
$files = Get-ChildItem -Path $StagingDir -Recurse -File | ForEach-Object {
    $_.FullName.Substring($StagingDir.Length + 1)
}

# install.bat phải chạy sau cùng (AppLaunched) nên không cần đứng đầu danh sách,
# nhưng vẫn phải nằm trong danh sách file được nhúng.
$fileLines = New-Object System.Collections.Generic.List[string]
$sourceLines = New-Object System.Collections.Generic.List[string]

for ($i = 0; $i -lt $files.Count; $i++) {
    $fileLines.Add("FILE$i=`"$($files[$i])`"")
    $sourceLines.Add("%FILE$i%=")
}

$sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3

[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=%InstallPrompt%
DisplayLicense=%DisplayLicense%
FinishMessage=%FinishMessage%
TargetName=%TargetName%
FriendlyName=%FriendlyName%
AppLaunched=%AppLaunched%
PostInstallCmd=%PostInstallCmd%
AdminQuietInstCmd=%AdminQuietInstCmd%
UserQuietInstCmd=%UserQuietInstCmd%
SourceFiles=SourceFiles

[Strings]
InstallPrompt=
DisplayLicense=
FinishMessage=
TargetName=$OutputExe
FriendlyName=TrashPet AI Desktop Assistant Setup
AppLaunched=installer_helper.exe
PostInstallCmd=<None>
AdminQuietInstCmd=installer_helper.exe
UserQuietInstCmd=installer_helper.exe
$($fileLines -join "`r`n")

[SourceFiles]
SourceFiles0=$StagingDir\

[SourceFiles0]
$($sourceLines -join "`r`n")
"@

Set-Content -Path $sedPath -Value $sed -Encoding ASCII

Write-Host "Building installer with IExpress..."
$proc = Start-Process -FilePath "$env:WINDIR\System32\iexpress.exe" `
    -ArgumentList "/N","/Q","trashpetai_installer.sed" -PassThru -Wait `
    -WorkingDirectory $PSScriptRoot `
    -RedirectStandardOutput "$PSScriptRoot\iexpress_out.log" `
    -RedirectStandardError "$PSScriptRoot\iexpress_err.log"
Write-Host "iexpress exit code: $($proc.ExitCode)"

if (Test-Path $OutputExe) {
    Write-Host "OK: $OutputExe"
} else {
    Write-Host "FAILED: output exe not found"
    exit 1
}
