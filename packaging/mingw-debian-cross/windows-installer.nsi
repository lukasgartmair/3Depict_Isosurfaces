; Script generated by the HM NIS Edit Script Wizard.

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "3Depict"
!define PRODUCT_VERSION "0.0.14"
!define PRODUCT_PUBLISHER "D. Haley, A. Ceguerra"
!define PRODUCT_WEB_SITE "http://threedepict.sourceforge.net"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\3Depict.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "COPYING"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\3Depict.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "Setup.exe"
InstallDir "$PROGRAMFILES\3Depict"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "3Depict program" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "src\3Depict.exe"
  File "src\wxmsw28u_richtext_gcc_custom.dll"
  File "src\wxmsw28u_qa_gcc_custom.dll"
  File "src\wxmsw28u_html_gcc_custom.dll"
  File "src\wxmsw28u_gl_gcc_custom.dll"
  File "src\wxmsw28u_core_gcc_custom.dll"
  File "src\wxmsw28u_aui_gcc_custom.dll"
  File "src\wxmsw28u_adv_gcc_custom.dll"
  File "src\wxbase28u_xml_gcc_custom.dll"
  File "src\wxbase28u_net_gcc_custom.dll"
  File "src\wxbase28u_gcc_custom.dll"
  File "src\libxml2-2.dll"
  File "src\libgslcblas-0.dll"
  File "src\libgsl-0.dll"
  File "src\libfreetype-6.dll"
  File "src\libintl-8.dll"
  File "src\libiconv-2.dll"
  File "src\libexpat-1.dll"
  File "src\libjpeg-8.dll"
  File "src\libpng12-0.dll"
  File "src\libtiff-4.dll"
  File "src\libz.so.1.2.7"
  File "src\libstdc++-6.dll"
  File "src\libmgl-5.dll"
  File "src\libgcc_s_sjlj-1.dll"
  File "docs\manual-latex\manual.pdf"

  CreateDirectory "$SMPROGRAMS\3Depict"
  CreateDirectory "$SMPROGRAMS\3Depict\textures"
  CreateShortCut "$SMPROGRAMS\3Depict\3Depict.lnk" "$INSTDIR\3Depict.exe"
  CreateShortCut "$DESKTOP\3Depict.lnk" "$INSTDIR\3Depict.exe"
  
  
  ;Language translations installation
  CreateDirectory "locales"
  SetOverwrite try
  SetOutPath "$INSTDIR\locales\"
  File /r locales\*.*
  
  SetOutPath "$INSTDIR\textures\"
  File "data\textures\animProgress0.png"
  File "data\textures\animProgress1.png"
  File "data\textures\animProgress2.png"
  File "data\textures\enlarge.png"
  File "data\textures\keyboard-alt.png"
  File "data\textures\keyboard-command.png"
  File "data\textures\keyboard-ctrl.png"
  File "data\textures\keyboard-shift.png"
  File "data\textures\keyboard-tab.png"
  File "data\textures\Left_clicked_mouse.png"
  File "data\textures\Left-Right-arrow.png"
  File "data\textures\middle_clicked_mouse.png"
  File "data\textures\Right-arrow.png"
  File "data\textures\Right_clicked_mouse.png"
  File "data\textures\rotateArrow.png"
  File "data\textures\scroll_wheel_mouse.png"
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
  Delete "$INSTDIR\textures\uninst.exe"
  
  Delete "$INSTDIR\textures\animProgress0.png"
  Delete "$INSTDIR\textures\animProgress1.png"
  Delete "$INSTDIR\textures\animProgress2.png"
  Delete "$INSTDIR\textures\scroll_wheel_mouse.png"
  Delete "$INSTDIR\textures\rotateArrow.png"
  Delete "$INSTDIR\textures\Right_clicked_mouse.png"
  Delete "$INSTDIR\textures\Right-arrow.png"
  Delete "$INSTDIR\textures\middle_clicked_mouse.png"
  Delete "$INSTDIR\textures\Left_clicked_mouse.png"
  Delete "$INSTDIR\textures\Left-Right-arrow.png"
  Delete "$INSTDIR\textures\keyboard-tab.png"
  Delete "$INSTDIR\textures\keyboard-shift.png"
  Delete "$INSTDIR\textures\keyboard-ctrl.png"
  Delete "$INSTDIR\textures\keyboard-command.png"
  Delete "$INSTDIR\textures\keyboard-alt.png"
  Delete "$INSTDIR\textures\enlarge.png"
 
  Delete "$INSTDIR\3Depict.exe"

  Delete "$INSTDIR\manual.pdf"
  RMDir /r "$INSTDIR\locales\*.*"
  RMDir "$INSTDIR\locales"

  Delete "$INSTDIR\wxmsw28u_richtext_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_qa_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_html_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_gl_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_core_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_aui_gcc_custom.dll"
  Delete "$INSTDIR\wxmsw28u_adv_gcc_custom.dll"
  Delete "$INSTDIR\wxbase28u_xml_gcc_custom.dll"
  Delete "$INSTDIR\wxbase28u_net_gcc_custom.dll"
  Delete "$INSTDIR\wxbase28u_gcc_custom.dll"
  Delete "$INSTDIR\libxml2-2.dll"
  Delete "$INSTDIR\libgslcblas-0.dll"
  Delete "$INSTDIR\libgsl-0.dll"
  Delete "$INSTDIR\libfreetype-6.dll"
  Delete "$INSTDIR\libintl-8.dll"
  Delete "$INSTDIR\libiconv-2.dll"
  Delete "$INSTDIR\libexpat-1.dll"
  Delete "$INSTDIR\libjpeg-8.dll"
  Delete "$INSTDIR\libpng12-0.dll"
  Delete "$INSTDIR\libtiff-4.dll"
  Delete "$INSTDIR\libz.so.1.2.7"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libmgl-5.dll"
  Delete "$INSTDIR\libgcc_s_sjlj-1.dll"

  Delete "$INSTDIR\uninst.exe"  
 
  Delete "$SMPROGRAMS\3Depict\Uninstall.lnk"
  Delete "$SMPROGRAMS\3Depict\Website.lnk"
  Delete "$DESKTOP\3Depict.lnk"
  Delete "$SMPROGRAMS\3Depict\3Depict.lnk"
  RMDir "$SMPROGRAMS\3Depict"
  
  RMDir "$INSTDIR\textures"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd