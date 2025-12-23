@echo off
rem Batch file for making slides for LackPaint
rem Must be run in the directory with the source JPGs, destination for the BMPs on the commandline

rem So var inside () blocks are expanded at runtime (with !'s) (https://superuser.com/questions/78496/variables-in-batch-file-not-being-set-when-inside-if):
setlocal enabledelayedexpansion

rem *** What the slide image should look like ***
rem image_action is either 
rem   dither - Images are dithered monochrome *OR*
rem   grey   - Images are 4-bit-per-pixel greyscale (.
set image_action=dither

rem ***NOTE: resizing preserves proportions
rem *** How a Portrait slide image should be resized ***
rem portrait_action is either 
rem   skip   - Skip the file. Image is not in slideshow. *OR*
rem   width  - Set the width to fit. Only the middle slice will be drawn.
rem   height - Set the height to fit. There may be whitespace either side.
set portrait_action=height

rem *** How a Landscape slide image should be resized ***
rem landscape_action is either 
rem   height - Set the height to fit. There may be clipping/whitespace either side. *OR*
rem   width  - Set the width to fit. Only the middle slice will be drawn.
set landscape_action=height

rem *** What to append an alternative title to the slideshow image file ***
rem append_action is 1 or 0 to enable or disable appending a title
rem append_string (set below) is either empty, which will append nothing, the title will be file's 8.3 name *OR*
rem any combination of these:
rem  !name_ext! - file JPG name with extension
rem  !name!     - file JPG name without extension
rem from EXIF:DateTime:
rem  !yyyy!  - file year  eg 2024
rem  !yy!    - file year  eg 24
rem  !mm!    - file month eg 08
rem  !mmm!   - file month eg Aug
rem  !dd!    - file date  eg 23
rem plus any characters, things like ( and ) need to be escaped with ^
rem Must be < 40 chars (see SLIDE_APPENDED_TEXT_MAX_LEN)
set append_action=1
rem append_string (set below)

set destination_path="%1"
if "%destination_path%" == "" (
    echo "Syntax: make_slides <dest path>"
    exit /b 1   
)

rem size of slide window
set slide_width=396
set slide_height=218

rem Allow full path to be specified:
set magick_exe=magick
rem Clean the destination dir
pushd "%destination_path%"
del *.jpg *.jpeg *.png *.bmp >nul 2>&1
popd
for %%J in (*.jpg *.jpeg) do (
  echo %%J
  copy "%%~J" "%destination_path%" /Y >nul
  pushd "%destination_path%"
  rem Force orientation so width vs height test is reliable:
  %magick_exe% mogrify -auto-orient "%%~J"
  rem Extract the value of width>height as 1/0 for landscape/portrait (treat square-ish as portrait):
  for /f "tokens=1" %%A in ('%magick_exe% identify -format "%%[fx:w>h*1.25]" "%%~J"') do (
    set landscape_image=%%A
  )

  rem Extract the EXIF year, month and date.
  rem (may fail)
  set yyyy=?
  set yy=?
  set mm=?
  set mmm=?
  set dd=?
  rem (3 tokens, delimited by : and space)
  for /f "tokens=1,2,3 delims=: " %%A in ('%magick_exe% identify -quiet -format "%%[EXIF:DateTime]" "%%~J"') do (
    rem Better names
    set yyyy=%%A
    set yy=!yyyy:~2,2!
    set mm=%%B
    set dd=%%C
    rem Month name (ugly but works):
    if        "!mm!"=="01" (
      set mmm=Jan
    ) else if "!mm!"=="02" (
      set mmm=Feb
    ) else if "!mm!"=="03" (
      set mmm=Mar
    ) else if "!mm!"=="04" (
      set mmm=Apr
    ) else if "!mm!"=="05" (
      set mmm=May
    ) else if "!mm!"=="06" (
      set mmm=Jun
    ) else if "!mm!"=="07" (
      set mmm=Jul
    ) else if "!mm!"=="08" (
      set mmm=Aug
    ) else if "!mm!"=="09" (
      set mmm=Sep
    ) else if "!mm!"=="10" (
      set mmm=Ocy
    ) else if "!mm!"=="11" (
      set mmm=Nov
    ) else if "!mm!"=="12" (
      set mmm=Dec
    ) else                 (
      set mmm=!mm!
    )
  )
  rem Name
  set name_ext=%%~J
  set name=%%~nJ

  if "!landscape_image!" == "1" (
    rem Landscape
    if "%landscape_action%" == "height" (
      set size_action_string=x!slide_height!
    ) else if "%landscape_action%" == "width" (
      set size_action_string=!slide_width!x
    ) else (
      popd
      echo Invalid landscape_action=%landscape_action%
      exit /b 1 
    )
  ) else (
    rem Portrait
    if "%portrait_action%" == "skip" (
      set size_action_string=none
    ) else if "%portrait_action%" == "height" (
      set size_action_string=x!slide_height!
    ) else if "%portrait_action%" == "width" (
      set size_action_string=!slide_width!x
    ) else (
      popd
      echo Invalid portrait_action=%portrait_action%
      exit /b 1 
    )
  )

  rem Skip too-small files
  for /f "tokens=1" %%A in ('%magick_exe% identify -format "%%[fx:w<!slide_width! && h<!slide_height!]" "%%~J"') do (
    if "%%A" == "1" set size_action_string=none
  )
  
  rem Make the slideshow image
  if not "!size_action_string!" == "none" (
    if "%image_action%" == "dither" (
      rem Mogrify it as 3x3 dither. Resizes. Replaces JPG:
      %magick_exe% mogrify -resize !size_action_string! -ordered-dither o3x3 -monochrome "%%~J"
      rem Creates uncompressed 1bpp BMP from JPG. %~n1 is just the filename part of %1 (the JPG) without quotes:
      %magick_exe% "%%~J" -alpha off -depth 1 -type bilevel -compress none BMP3:"%%~nJ.BMP"
      rem Remove JPG
      del "%%~J"
    ) else if "%image_action%" == "grey" (
      rem Resizes. Replaces JPG:
      %magick_exe% mogrify -resize !size_action_string! "%%~J"
      rem Convert JPG to n-bit greyscale PNG
      %magick_exe% "%%~J" -colorspace gray -depth 4 -define png:color-type=0 -define png:bit-depth=4 "%%~nJ.PNG"
      rem Creates uncompressed n-bpp BMP from PNG.
      %magick_exe% "%%~nJ.PNG" -alpha off -depth 4 -compress none BMP3:"%%~nJ.BMP"
      rem Remove JPG & PNG:
      del "%%~J"
      del "%%~nJ.PNG"    
    ) else (
      popd
      echo Invalid image_action=%image_action%
      exit /b 1 
    )

    rem Optionally append file name/date
    if "!append_action!"=="1" (
      if "!yyyy!"=="?" (
        rem "Original Name.JPG" (no date)
        set append_string=!name!
      ) else (
        rem "Original Name.JPG (Aug '24)"
        set append_string=!name! ^(!mmm! '!yy!^)
      )
      if not "!append_string!"=="" (
        echo NAME:=>>"%%~nJ.BMP"
        echo !append_string!>>"%%~nJ.BMP"
      )
    )
  ) else (
    rem Skipped 
    del "%%~J"
    echo ^(skipped^)
  )
  popd
)
