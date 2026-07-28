//
//  AppState.cpp
//  OpenKey cho Linux
//

#include "AppState.h"

// --- Dinh nghia that cho cac bien engine khai bao extern trong Engine.h ------
// Gia tri o day la mac dinh luc chua co file cau hinh; Config se ghi de sau.

int vLanguage = 1;                  // 1: tieng Viet
int vInputType = 0;                 // 0: Telex
int vFreeMark = 0;
int vCodeTable = 0;                 // 0: Unicode dung san
int vSwitchKeyStatus = openkey::kDefaultSwitchKeyStatus;
int vCheckSpelling = 1;
int vUseModernOrthography = 0;      // 0: oa, uy kieu cu
int vQuickTelex = 0;
int vRestoreIfWrongSpelling = 0;
int vFixRecommendBrowser = 1;
int vUseMacro = 1;
int vUseMacroInEnglishMode = 1;
int vAutoCapsMacro = 0;
int vUseSmartSwitchKey = 1;
int vUpperCaseFirstChar = 0;
int vTempOffSpelling = 1;
int vAllowConsonantZFWJ = 0;
int vQuickStartConsonant = 0;
int vQuickEndConsonant = 0;
int vRememberCode = 1;
int vOtherLanguage = 1;
int vTempOffOpenKey = 1;

namespace openkey {

void resetAppStateToDefault() {
    vLanguage = 1;
    vInputType = 0;
    vFreeMark = 0;
    vCodeTable = 0;
    vSwitchKeyStatus = kDefaultSwitchKeyStatus;
    vCheckSpelling = 1;
    vUseModernOrthography = 0;
    vQuickTelex = 0;
    vRestoreIfWrongSpelling = 0;
    vFixRecommendBrowser = 1;
    vUseMacro = 1;
    vUseMacroInEnglishMode = 1;
    vAutoCapsMacro = 0;
    vUseSmartSwitchKey = 1;
    vUpperCaseFirstChar = 0;
    vTempOffSpelling = 1;
    vAllowConsonantZFWJ = 0;
    vQuickStartConsonant = 0;
    vQuickEndConsonant = 0;
    vRememberCode = 1;
    vOtherLanguage = 1;
    vTempOffOpenKey = 1;
}

} // namespace openkey
