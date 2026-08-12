#ifndef PLATFORM_PROFILE_H
#define PLATFORM_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * F28335_RTControl_Platform — Product Profile
 *
 * Exactly one product profile MUST be defined at compile time.  The profile
 * is a compile-time constant; production binaries MUST NOT switch profiles
 * via runtime communication commands.
 *
 * Legal define-before-include:
 *   PLATFORM_PROFILE_PROTOTYPE    — fast dev, debug visible, dev defaults
 *   PLATFORM_PROFILE_INDUSTRIAL   — fail-safe, strict validation, release
 *
 * If neither or both are defined → #error at compile time.
 *=========================================================================*/

/* ---- Guard: exactly one profile must be selected ---- */
#if defined(PLATFORM_PROFILE_PROTOTYPE) && defined(PLATFORM_PROFILE_INDUSTRIAL)
#error "PLATFORM_PROFILE: both PROTOTYPE and INDUSTRIAL defined — choose exactly one."
#endif

#if !defined(PLATFORM_PROFILE_PROTOTYPE) && !defined(PLATFORM_PROFILE_INDUSTRIAL)
#error "PLATFORM_PROFILE: no profile defined — define PLATFORM_PROFILE_PROTOTYPE or PLATFORM_PROFILE_INDUSTRIAL."
#endif

/* ---- Profile enum (for diagnostic use) ---- */
typedef enum
{
    PLATFORM_PROFILE_ID_PROTOTYPE  = 0,
    PLATFORM_PROFILE_ID_INDUSTRIAL = 1
} PlatformProfileId;

/* ---- Resolve active profile to a compile-time constant ---- */
#if defined(PLATFORM_PROFILE_PROTOTYPE)
    #define PLATFORM_PROFILE_ACTIVE  PLATFORM_PROFILE_ID_PROTOTYPE
    #define PLATFORM_PROFILE_NAME    "Prototype"
#elif defined(PLATFORM_PROFILE_INDUSTRIAL)
    #define PLATFORM_PROFILE_ACTIVE  PLATFORM_PROFILE_ID_INDUSTRIAL
    #define PLATFORM_PROFILE_NAME    "Industrial"
#endif

/* ---- Capability macros derived from profile ---- */
#if defined(PLATFORM_PROFILE_PROTOTYPE)
    /* Prototype: rich diagnostics, dev defaults, safe-but-quick recovery */
    #define PLATFORM_CAP_DIAGNOSTICS_EXTENDED   1
    #define PLATFORM_CAP_SAFE_OPENLOOP          1
    #define PLATFORM_CAP_ALGORITHM_BYPASS       1
    #define PLATFORM_CAP_FAULT_QUICK_RESET      1
    #define PLATFORM_CAP_DEV_DEFAULTS           1
    #define PLATFORM_CAP_DEBUGGER_HALT_ALLOWED  1
    #define PLATFORM_CAP_NONCRITICAL_DOWNGRADE  1
    #define PLATFORM_IS_PRODUCTION_RELEASE      0
#else
    /* Industrial: strict validation, locked debug, production release */
    #define PLATFORM_CAP_DIAGNOSTICS_EXTENDED   0
    #define PLATFORM_CAP_SAFE_OPENLOOP          0
    #define PLATFORM_CAP_ALGORITHM_BYPASS       0
    #define PLATFORM_CAP_FAULT_QUICK_RESET      0
    #define PLATFORM_CAP_DEV_DEFAULTS           0
    #define PLATFORM_CAP_DEBUGGER_HALT_ALLOWED  0
    #define PLATFORM_CAP_NONCRITICAL_DOWNGRADE  0
    #define PLATFORM_IS_PRODUCTION_RELEASE      1
#endif

/* Minimal safety line: these are NEVER disabled regardless of profile.
 * Each module reads its own capability; no scattered #ifdef INDUSTRIAL. */
#define PLATFORM_SAFETY_PWM_DEFAULT_OFF        1
#define PLATFORM_SAFETY_DUTY_CYCLE_CLAMP       1
#define PLATFORM_SAFETY_TRIP_ZONE_ENABLED       1
#define PLATFORM_SAFETY_PLL_INIT_CHECK          1
#define PLATFORM_SAFETY_COMM_RANGE_CHECK        1
#define PLATFORM_SAFETY_ISR_NO_ALLOC            1

/*===========================================================================
 * Binary Identity — reserved, real interfaces
 *
 * These macros provide identity information embedded in the binary.  Some
 * values require build-system injection; fallback strategies are documented
 * for each field.
 *=========================================================================*/

/* ---- Platform version (semantic, maintained manually) ---- */
#ifndef PLATFORM_VERSION_MAJOR
#define PLATFORM_VERSION_MAJOR  1
#endif
#ifndef PLATFORM_VERSION_MINOR
#define PLATFORM_VERSION_MINOR  0
#endif
#ifndef PLATFORM_VERSION_PATCH
#define PLATFORM_VERSION_PATCH  0
#endif

#define PLATFORM_VERSION_STRING  "1.0.0"

/* ---- Build timestamp (auto from compiler) ---- */
#define PLATFORM_BUILD_DATE      __DATE__
#define PLATFORM_BUILD_TIME      __TIME__

/* ---- Git commit hash — requires -D injection from build system ----
 * Fallback: "unknown" when not injected.  The quality-gate script
 * verifies that Release builds always inject a real commit hash. */
#ifndef PLATFORM_GIT_COMMIT
#define PLATFORM_GIT_COMMIT      "unknown"
#endif

/* ---- Configuration CRC — requires build-system computation ----
 * Fallback: 0x0000 when not computed.  Set via -D PLATFORM_CONFIG_CRC=0xXXXX
 * after computing a CRC over the effective build configuration. */
#ifndef PLATFORM_CONFIG_CRC
#define PLATFORM_CONFIG_CRC      0x0000U
#endif

/* ---- Build identity string (composite, for diagnostic output) ---- */
#define PLATFORM_IDENTITY_STRING  PLATFORM_PROFILE_NAME \
    " v" PLATFORM_VERSION_STRING \
    " " PLATFORM_BUILD_DATE " " PLATFORM_BUILD_TIME \
    " [" PLATFORM_GIT_COMMIT "]"

/*===========================================================================
 * Build configuration name — used to construct unique artifact names.
 *
 * Combines profile + load-target to ensure Prototype firmware cannot be
 * mistaken for an Industrial release.
 *
 * Expected values:
 *   "Prototype_RAM_Debug"
 *   "Prototype_Flash_Demo"
 *   "Industrial_RAM_Debug"
 *   "Industrial_Flash_Release"
 *=========================================================================*/
#if defined(FLASH)
    #if defined(PLATFORM_PROFILE_PROTOTYPE)
        #define PLATFORM_BUILD_ID  "Prototype_Flash_Demo"
    #elif defined(PLATFORM_PROFILE_INDUSTRIAL)
        #define PLATFORM_BUILD_ID  "Industrial_Flash_Release"
    #endif
#else
    #if defined(PLATFORM_PROFILE_PROTOTYPE)
        #define PLATFORM_BUILD_ID  "Prototype_RAM_Debug"
    #elif defined(PLATFORM_PROFILE_INDUSTRIAL)
        #define PLATFORM_BUILD_ID  "Industrial_RAM_Debug"
    #endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_PROFILE_H */
