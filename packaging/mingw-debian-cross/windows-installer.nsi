; Script generated by the HM NIS Edit Script Wizard.

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "3Depict"
!define PRODUCT_VERSION "0.0.19"
!define PRODUCT_PUBLISHER "D. Haley, A. Ceguerra"
!define PRODUCT_WEB_SITE "http://threedepict.sourceforge.net"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\3Depict.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor /FINAL /SOLID lzma
SetCompressorDictSize 64

!include "x64.nsh"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "src/myAppIcon.ico"
!define MUI_UNICON "src/myAppIcon.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "data/textures/tex-source/3Depict-icon-hires.png"
!define MUI_HEADERIMAGE_RIGHT

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "COPYING"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
;!define MUI_FINISHPAGE_RUN "$INSTDIR\3Depict.exe"
;!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "3Depict-setup.exe"
InstallDir "$PROGRAMFILES64\3Depict"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "3Depict program" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "src\3Depict.exe"
  File "docs\manual-latex\manual.pdf"
  ;INSERT_DLLS_HERE

  CreateDirectory "$SMPROGRAMS\3Depict"
  CreateShortCut "$SMPROGRAMS\3Depict\3Depict.lnk" "$INSTDIR\3Depict.exe"
  CreateShortCut "$DESKTOP\3Depict.lnk" "$INSTDIR\3Depict.exe"
  
  
  ;Language translations installation
  CreateDirectory "locales"
  SetOverwrite try
  SetOutPath "$INSTDIR\locales\"
  File /r locales\*.*
  
  SetOutPath "$INSTDIR\textures\"
  File "data\textures\Left-Right-arrow.png"
  File "data\textures\Left_clicked_mouse.png"
  File "data\textures\Right-arrow.png"
  File "data\textures\Right_clicked_mouse.png"
  File "data\textures\animProgress0.png"
  File "data\textures\animProgress1.png"
  File "data\textures\animProgress2.png"
  File "data\textures\enlarge.png"
  File "data\textures\keyboard-alt.png"
  File "data\textures\keyboard-command.png"
  File "data\textures\keyboard-ctrl.png"
  File "data\textures\keyboard-shift.png"
  File "data\textures\keyboard-tab.png"
  File "data\textures\middle_clicked_mouse.png"
  File "data\textures\plot_slide_x.png"
  File "data\textures\plot_zoom_reset.png"
  File "data\textures\plot_zoom_x.png"
  File "data\textures\plot_zoom_y.png"
  File "data\textures\rotateArrow.png"
  File "data\textures\scroll_wheel_mouse.png"
  SetOutPath "$INSTDIR"
  File "data\3Depict.xpm"
  File "data\atomic-mass-table.dtd"
  File "data\naturalAbundance.xml"
  File "data\startup-tips.txt"
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\3Depict\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\3Depict\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\3Depict.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\3Depict.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name)?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\uninst.exe"  
  
  RMDir /r "$INSTDIR\textures"
  RMDir "$INSTDIR\textures"
  RMDir /r "$INSTDIR\locales"
  RMDir "$INSTDIR\locales"
  
  Delete "$INSTDIR\3Depict.xpm"
  Delete "$INSTDIR\atomic-mass-table.dtd"
  Delete "$INSTDIR\naturalAbundance.xml"
  Delete "$INSTDIR\startup-tips.txt"
 
  Delete "$INSTDIR\3Depict.exe"

  Delete "$INSTDIR\manual.pdf"

  ;This is a token that should be replaced with the DLLS to uninstall
  ;INSERT_UNINST_DLLS_HERE

 
  Delete "$SMPROGRAMS\3Depict\Uninstall.lnk"
  Delete "$SMPROGRAMS\3Depict\Website.lnk"
  Delete "$SMPROGRAMS\3Depict\manual.pdf"
  Delete "$DESKTOP\3Depict.lnk"
  Delete "$SMPROGRAMS\3Depict\3Depict.lnk"
  RMDir "$SMPROGRAMS\3Depict"
  
  RMDir "$INSTDIR\textures"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
