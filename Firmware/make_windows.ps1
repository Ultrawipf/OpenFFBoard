# 1. Setup the entry point
$stPath = "C:\ST"

Write-Host "--- Looking for compilation tools in $stPath ---" -ForegroundColor Cyan

# 2. Find folder 'bin' which contain the make.exe (the lastest one)
$makeBin = Get-ChildItem -Path $stPath -Filter "make.exe" -Recurse -ErrorAction SilentlyContinue | 
           Sort-Object LastWriteTime -Descending | 
           Select-Object -First 1 -ExpandProperty DirectoryName

# 3. find folder 'bin' for arm-none-eabi-gcc.exe (the lastest one)
$gccBin = Get-ChildItem -Path $stPath -Filter "arm-none-eabi-gcc.exe" -Recurse -ErrorAction SilentlyContinue | 
          Sort-Object LastWriteTime -Descending | 
          Select-Object -First 1 -ExpandProperty DirectoryName

if ($makeBin -and $gccBin) {
    Write-Host "[OK] Make found : $makeBin" -ForegroundColor Green
    Write-Host "[OK] GCC found : $gccBin" -ForegroundColor Green

    # 4. Add paths to PATH file
    $env:PATH = "$makeBin;$gccBin;" + $env:PATH

    Write-Host "--- Launch compilation for all target ---" -ForegroundColor Yellow
    
    # 5. Make all targets
    make MCU_TARGET=F407VG clean
    make MCU_TARGET=F407VG -j8
    make MCU_TARGET=F407VG_DISCO clean
    make MCU_TARGET=F407VG_DISCO -j8
    make MCU_TARGET=F411RE clean
    make MCU_TARGET=F411RE -j8
} else {
    Write-Host "[ERROR] Can't detect make tools. Check than STM32CubeIDE is in directory $stPath" -ForegroundColor Red
}