/**
 * Nova3D Editor Design System - Design Tokens JavaScript API
 * Provides programmatic access to design tokens and theme switching
 * @version 2.0
 */

const DesignTokens = (function() {
    'use strict';

    // =========================================================================
    // Color Primitives
    // =========================================================================
    const colors = {
        gray: {
            50: '#fafafa', 100: '#f5f5f5', 200: '#e5e5e5', 300: '#d4d4d4',
            400: '#a3a3a3', 500: '#737373', 600: '#525252', 700: '#404040',
            800: '#262626', 900: '#171717', 950: '#0a0a0a'
        },
        blue: {
            50: '#eff6ff', 100: '#dbeafe', 200: '#bfdbfe', 300: '#93c5fd',
            400: '#60a5fa', 500: '#3b82f6', 600: '#2563eb', 700: '#1d4ed8',
            800: '#1e40af', 900: '#1e3a8a'
        },
        green: {
            50: '#f0fdf4', 100: '#dcfce7', 200: '#bbf7d0', 300: '#86efac',
            400: '#4ade80', 500: '#22c55e', 600: '#16a34a', 700: '#15803d',
            800: '#166534', 900: '#14532d'
        },
        yellow: {
            50: '#fefce8', 100: '#fef9c3', 200: '#fef08a', 300: '#fde047',
            400: '#facc15', 500: '#eab308', 600: '#ca8a04', 700: '#a16207',
            800: '#854d0e', 900: '#713f12'
        },
        red: {
            50: '#fef2f2', 100: '#fee2e2', 200: '#fecaca', 300: '#fca5a5',
            400: '#f87171', 500: '#ef4444', 600: '#dc2626', 700: '#b91c1c',
            800: '#991b1b', 900: '#7f1d1d'
        },
        cyan: {
            50: '#ecfeff', 100: '#cffafe', 200: '#a5f3fc', 300: '#67e8f9',
            400: '#22d3ee', 500: '#06b6d4', 600: '#0891b2', 700: '#0e7490',
            800: '#155e75', 900: '#164e63'
        },
        purple: {
            50: '#faf5ff', 100: '#f3e8ff', 200: '#e9d5ff', 300: '#d8b4fe',
            400: '#c084fc', 500: '#a855f7', 600: '#9333ea', 700: '#7e22ce',
            800: '#6b21a8', 900: '#581c87'
        },
        orange: {
            50: '#fff7ed', 100: '#ffedd5', 200: '#fed7aa', 300: '#fdba74',
            400: '#fb923c', 500: '#f97316', 600: '#ea580c', 700: '#c2410c',
            800: '#9a3412', 900: '#7c2d12'
        }
    };

    // =========================================================================
    // Typography
    // =========================================================================
    const typography = {
        fontFamily: {
            sans: "-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', 'Oxygen', 'Ubuntu', 'Cantarell', 'Fira Sans', 'Droid Sans', 'Helvetica Neue', sans-serif",
            mono: "'JetBrains Mono', 'Fira Code', 'SF Mono', 'Consolas', 'Monaco', 'Andale Mono', 'Ubuntu Mono', monospace",
            display: "Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif"
        },
        fontSize: {
            xs: '0.6875rem',   // 11px
            sm: '0.75rem',     // 12px
            base: '0.8125rem', // 13px
            md: '0.875rem',    // 14px
            lg: '1rem',        // 16px
            xl: '1.125rem',    // 18px
            '2xl': '1.25rem',  // 20px
            '3xl': '1.5rem',   // 24px
            '4xl': '1.875rem'  // 30px
        },
        fontWeight: {
            light: 300,
            normal: 400,
            medium: 500,
            semibold: 600,
            bold: 700
        },
        lineHeight: {
            none: 1,
            tight: 1.25,
            snug: 1.375,
            normal: 1.5,
            relaxed: 1.625,
            loose: 2
        }
    };

    // =========================================================================
    // Spacing
    // =========================================================================
    const spacing = {
        0: '0',
        px: '1px',
        0.5: '0.125rem',
        1: '0.25rem',
        1.5: '0.375rem',
        2: '0.5rem',
        2.5: '0.625rem',
        3: '0.75rem',
        3.5: '0.875rem',
        4: '1rem',
        5: '1.25rem',
        6: '1.5rem',
        7: '1.75rem',
        8: '2rem',
        9: '2.25rem',
        10: '2.5rem',
        12: '3rem',
        14: '3.5rem',
        16: '4rem',
        20: '5rem',
        24: '6rem'
    };

    // =========================================================================
    // Border Radius
    // =========================================================================
    const borderRadius = {
        none: '0',
        sm: '0.125rem',
        default: '0.25rem',
        md: '0.375rem',
        lg: '0.5rem',
        xl: '0.75rem',
        '2xl': '1rem',
        '3xl': '1.5rem',
        full: '9999px'
    };

    // =========================================================================
    // Shadows
    // =========================================================================
    const shadows = {
        xs: '0 1px 2px 0 rgb(0 0 0 / 0.05)',
        sm: '0 1px 3px 0 rgb(0 0 0 / 0.1), 0 1px 2px -1px rgb(0 0 0 / 0.1)',
        default: '0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1)',
        md: '0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1)',
        lg: '0 10px 15px -3px rgb(0 0 0 / 0.1), 0 4px 6px -4px rgb(0 0 0 / 0.1)',
        xl: '0 20px 25px -5px rgb(0 0 0 / 0.1), 0 8px 10px -6px rgb(0 0 0 / 0.1)',
        '2xl': '0 25px 50px -12px rgb(0 0 0 / 0.25)',
        inner: 'inset 0 2px 4px 0 rgb(0 0 0 / 0.05)',
        none: '0 0 #0000'
    };

    // =========================================================================
    // Transitions
    // =========================================================================
    const transitions = {
        duration: {
            instant: 0,
            fast: 75,
            normal: 150,
            moderate: 200,
            slow: 300,
            slower: 500,
            slowest: 700
        },
        easing: {
            linear: 'linear',
            in: 'cubic-bezier(0.4, 0, 1, 1)',
            out: 'cubic-bezier(0, 0, 0.2, 1)',
            inOut: 'cubic-bezier(0.4, 0, 0.2, 1)',
            bounce: 'cubic-bezier(0.68, -0.55, 0.265, 1.55)',
            elastic: 'cubic-bezier(0.68, -0.6, 0.32, 1.6)'
        }
    };

    // =========================================================================
    // Z-Index
    // =========================================================================
    const zIndex = {
        behind: -1,
        base: 0,
        docked: 10,
        dropdown: 100,
        sticky: 200,
        fixed: 300,
        overlay: 400,
        modal: 500,
        popover: 600,
        tooltip: 700,
        notification: 800,
        top: 900,
        max: 9999
    };

    // =========================================================================
    // Breakpoints
    // =========================================================================
    const breakpoints = {
        xs: 480,
        sm: 640,
        md: 768,
        lg: 1024,
        xl: 1280,
        '2xl': 1536
    };

    // =========================================================================
    // Theme Definitions
    // =========================================================================
    const themes = {
        dark: {
            bg: {
                canvas: colors.gray[950],
                primary: colors.gray[900],
                secondary: colors.gray[800],
                tertiary: colors.gray[700],
                elevated: colors.gray[800],
                sunken: colors.gray[950],
                hover: colors.gray[700],
                active: colors.gray[600],
                selected: colors.blue[900],
                selectedHover: colors.blue[800],
                disabled: colors.gray[800]
            },
            border: {
                default: colors.gray[700],
                muted: colors.gray[800],
                emphasis: colors.gray[600],
                focus: colors.blue[500],
                error: colors.red[500],
                success: colors.green[500],
                warning: colors.yellow[500]
            },
            text: {
                primary: colors.gray[100],
                secondary: colors.gray[400],
                muted: colors.gray[500],
                disabled: colors.gray[600],
                inverse: colors.gray[900],
                link: colors.blue[400],
                linkHover: colors.blue[300],
                onAccent: '#ffffff'
            },
            accent: {
                primary: colors.blue[500],
                primaryHover: colors.blue[400],
                primaryActive: colors.blue[600],
                secondary: colors.purple[500],
                secondaryHover: colors.purple[400]
            },
            status: {
                success: colors.green[500],
                successBg: 'rgba(34, 197, 94, 0.15)',
                successText: colors.green[400],
                warning: colors.yellow[500],
                warningBg: 'rgba(234, 179, 8, 0.15)',
                warningText: colors.yellow[400],
                error: colors.red[500],
                errorBg: 'rgba(239, 68, 68, 0.15)',
                errorText: colors.red[400],
                info: colors.cyan[500],
                infoBg: 'rgba(6, 182, 212, 0.15)',
                infoText: colors.cyan[400]
            },
            scrollbar: {
                track: colors.gray[900],
                thumb: colors.gray[600],
                thumbHover: colors.gray[500]
            }
        },
        light: {
            bg: {
                canvas: colors.gray[100],
                primary: colors.gray[50],
                secondary: '#ffffff',
                tertiary: colors.gray[100],
                elevated: '#ffffff',
                sunken: colors.gray[200],
                hover: colors.gray[200],
                active: colors.gray[300],
                selected: colors.blue[100],
                selectedHover: colors.blue[200],
                disabled: colors.gray[100]
            },
            border: {
                default: colors.gray[300],
                muted: colors.gray[200],
                emphasis: colors.gray[400],
                focus: colors.blue[500],
                error: colors.red[500],
                success: colors.green[500],
                warning: colors.yellow[500]
            },
            text: {
                primary: colors.gray[900],
                secondary: colors.gray[600],
                muted: colors.gray[500],
                disabled: colors.gray[400],
                inverse: colors.gray[50],
                link: colors.blue[600],
                linkHover: colors.blue[700],
                onAccent: '#ffffff'
            },
            accent: {
                primary: colors.blue[500],
                primaryHover: colors.blue[600],
                primaryActive: colors.blue[700],
                secondary: colors.purple[500],
                secondaryHover: colors.purple[600]
            },
            status: {
                success: colors.green[600],
                successBg: 'rgba(34, 197, 94, 0.1)',
                successText: colors.green[700],
                warning: colors.yellow[600],
                warningBg: 'rgba(234, 179, 8, 0.1)',
                warningText: colors.yellow[700],
                error: colors.red[600],
                errorBg: 'rgba(239, 68, 68, 0.1)',
                errorText: colors.red[700],
                info: colors.cyan[600],
                infoBg: 'rgba(6, 182, 212, 0.1)',
                infoText: colors.cyan[700]
            },
            scrollbar: {
                track: colors.gray[200],
                thumb: colors.gray[400],
                thumbHover: colors.gray[500]
            }
        },
        highContrast: {
            bg: {
                canvas: '#000000',
                primary: '#000000',
                secondary: '#1a1a1a',
                tertiary: '#2a2a2a',
                elevated: '#1a1a1a',
                sunken: '#000000',
                hover: '#333333',
                active: '#444444',
                selected: '#004d99',
                selectedHover: '#0066cc',
                disabled: '#1a1a1a'
            },
            border: {
                default: '#ffffff',
                muted: '#888888',
                emphasis: '#ffffff',
                focus: '#00ffff',
                error: '#ff0000',
                success: '#00ff00',
                warning: '#ffff00'
            },
            text: {
                primary: '#ffffff',
                secondary: '#cccccc',
                muted: '#999999',
                disabled: '#666666',
                inverse: '#000000',
                link: '#00ccff',
                linkHover: '#66e0ff',
                onAccent: '#000000'
            },
            accent: {
                primary: '#00ccff',
                primaryHover: '#33d6ff',
                primaryActive: '#0099cc',
                secondary: '#ff00ff',
                secondaryHover: '#ff66ff'
            },
            status: {
                success: '#00ff00',
                successBg: 'rgba(0, 255, 0, 0.2)',
                successText: '#00ff00',
                warning: '#ffff00',
                warningBg: 'rgba(255, 255, 0, 0.2)',
                warningText: '#ffff00',
                error: '#ff0000',
                errorBg: 'rgba(255, 0, 0, 0.2)',
                errorText: '#ff0000',
                info: '#00ffff',
                infoBg: 'rgba(0, 255, 255, 0.2)',
                infoText: '#00ffff'
            },
            scrollbar: {
                track: '#000000',
                thumb: '#666666',
                thumbHover: '#888888'
            }
        }
    };

    // =========================================================================
    // Current Theme State
    // =========================================================================
    let currentTheme = 'dark';
    let customOverrides = {};
    const themeChangeCallbacks = [];

    // =========================================================================
    // Theme Management
    // =========================================================================

    /**
     * Get the current theme name
     * @returns {string}
     */
    function getTheme() {
        return currentTheme;
    }

    /**
     * Set the active theme
     * @param {string} themeName - 'dark', 'light', or 'highContrast'
     */
    function setTheme(themeName) {
        if (!themes[themeName]) {
            console.warn(`Theme "${themeName}" not found. Available themes: ${Object.keys(themes).join(', ')}`);
            return;
        }

        currentTheme = themeName;
        applyTheme(themes[themeName]);

        // Store preference
        try {
            localStorage.setItem('nova3d-editor-theme', themeName);
        } catch (e) {
            // localStorage may not be available
        }

        // Notify listeners
        themeChangeCallbacks.forEach(cb => cb(themeName));
    }

    /**
     * Apply theme tokens to CSS custom properties
     * @param {Object} theme
     */
    function applyTheme(theme) {
        const root = document.documentElement;

        // Apply background colors
        if (theme.bg) {
            root.style.setProperty('--bg-canvas', theme.bg.canvas);
            root.style.setProperty('--bg-primary', theme.bg.primary);
            root.style.setProperty('--bg-secondary', theme.bg.secondary);
            root.style.setProperty('--bg-tertiary', theme.bg.tertiary);
            root.style.setProperty('--bg-elevated', theme.bg.elevated);
            root.style.setProperty('--bg-sunken', theme.bg.sunken);
            root.style.setProperty('--bg-hover', theme.bg.hover);
            root.style.setProperty('--bg-active', theme.bg.active);
            root.style.setProperty('--bg-selected', theme.bg.selected);
            root.style.setProperty('--bg-selected-hover', theme.bg.selectedHover);
            root.style.setProperty('--bg-disabled', theme.bg.disabled);
        }

        // Apply border colors
        if (theme.border) {
            root.style.setProperty('--border-default', theme.border.default);
            root.style.setProperty('--border-muted', theme.border.muted);
            root.style.setProperty('--border-emphasis', theme.border.emphasis);
            root.style.setProperty('--border-focus', theme.border.focus);
            root.style.setProperty('--border-error', theme.border.error);
            root.style.setProperty('--border-success', theme.border.success);
            root.style.setProperty('--border-warning', theme.border.warning);
        }

        // Apply text colors
        if (theme.text) {
            root.style.setProperty('--text-primary', theme.text.primary);
            root.style.setProperty('--text-secondary', theme.text.secondary);
            root.style.setProperty('--text-muted', theme.text.muted);
            root.style.setProperty('--text-disabled', theme.text.disabled);
            root.style.setProperty('--text-inverse', theme.text.inverse);
            root.style.setProperty('--text-link', theme.text.link);
            root.style.setProperty('--text-link-hover', theme.text.linkHover);
            root.style.setProperty('--text-on-accent', theme.text.onAccent);
        }

        // Apply accent colors
        if (theme.accent) {
            root.style.setProperty('--accent-primary', theme.accent.primary);
            root.style.setProperty('--accent-primary-hover', theme.accent.primaryHover);
            root.style.setProperty('--accent-primary-active', theme.accent.primaryActive);
            root.style.setProperty('--accent-secondary', theme.accent.secondary);
            root.style.setProperty('--accent-secondary-hover', theme.accent.secondaryHover);
        }

        // Apply status colors
        if (theme.status) {
            root.style.setProperty('--status-success', theme.status.success);
            root.style.setProperty('--status-success-bg', theme.status.successBg);
            root.style.setProperty('--status-success-text', theme.status.successText);
            root.style.setProperty('--status-warning', theme.status.warning);
            root.style.setProperty('--status-warning-bg', theme.status.warningBg);
            root.style.setProperty('--status-warning-text', theme.status.warningText);
            root.style.setProperty('--status-error', theme.status.error);
            root.style.setProperty('--status-error-bg', theme.status.errorBg);
            root.style.setProperty('--status-error-text', theme.status.errorText);
            root.style.setProperty('--status-info', theme.status.info);
            root.style.setProperty('--status-info-bg', theme.status.infoBg);
            root.style.setProperty('--status-info-text', theme.status.infoText);
        }

        // Apply scrollbar colors
        if (theme.scrollbar) {
            root.style.setProperty('--scrollbar-track', theme.scrollbar.track);
            root.style.setProperty('--scrollbar-thumb', theme.scrollbar.thumb);
            root.style.setProperty('--scrollbar-thumb-hover', theme.scrollbar.thumbHover);
        }

        // Apply any custom overrides
        Object.entries(customOverrides).forEach(([key, value]) => {
            root.style.setProperty(key, value);
        });

        // Update body class for theme-specific CSS
        document.body.classList.remove('theme-dark', 'theme-light', 'theme-high-contrast');
        document.body.classList.add(`theme-${currentTheme.replace(/([A-Z])/g, '-$1').toLowerCase()}`);
    }

    /**
     * Register a callback for theme changes
     * @param {Function} callback
     * @returns {Function} Unsubscribe function
     */
    function onThemeChange(callback) {
        themeChangeCallbacks.push(callback);
        return () => {
            const index = themeChangeCallbacks.indexOf(callback);
            if (index > -1) themeChangeCallbacks.splice(index, 1);
        };
    }

    /**
     * Set custom token overrides
     * @param {Object} overrides - Map of CSS variable names to values
     */
    function setCustomTokens(overrides) {
        customOverrides = { ...customOverrides, ...overrides };
        applyTheme(themes[currentTheme]);
    }

    /**
     * Reset custom overrides
     */
    function resetCustomTokens() {
        customOverrides = {};
        applyTheme(themes[currentTheme]);
    }

    // =========================================================================
    // Token Accessors
    // =========================================================================

    /**
     * Get a CSS custom property value
     * @param {string} name - CSS variable name (with or without --)
     * @returns {string}
     */
    function getCSSVar(name) {
        const varName = name.startsWith('--') ? name : `--${name}`;
        return getComputedStyle(document.documentElement).getPropertyValue(varName).trim();
    }

    /**
     * Set a CSS custom property value
     * @param {string} name - CSS variable name (with or without --)
     * @param {string} value
     */
    function setCSSVar(name, value) {
        const varName = name.startsWith('--') ? name : `--${name}`;
        document.documentElement.style.setProperty(varName, value);
        customOverrides[varName] = value;
    }

    /**
     * Get color from the palette
     * @param {string} colorName - e.g., 'blue-500', 'gray-900'
     * @returns {string|undefined}
     */
    function getColor(colorName) {
        const [name, shade] = colorName.split('-');
        if (colors[name] && colors[name][shade]) {
            return colors[name][shade];
        }
        return undefined;
    }

    /**
     * Get spacing value
     * @param {number|string} size - Spacing key
     * @returns {string}
     */
    function getSpacing(size) {
        return spacing[size] || `${size}rem`;
    }

    /**
     * Generate a transition string
     * @param {Object} options
     * @returns {string}
     */
    function transition(options = {}) {
        const {
            property = 'all',
            duration = 'normal',
            easing = 'inOut'
        } = options;

        const dur = transitions.duration[duration] || duration;
        const ease = transitions.easing[easing] || easing;

        return `${property} ${dur}ms ${ease}`;
    }

    // =========================================================================
    // Utility Functions
    // =========================================================================

    /**
     * Convert hex to RGB
     * @param {string} hex
     * @returns {Object|null}
     */
    function hexToRgb(hex) {
        const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
        return result ? {
            r: parseInt(result[1], 16),
            g: parseInt(result[2], 16),
            b: parseInt(result[3], 16)
        } : null;
    }

    /**
     * Convert RGB to hex
     * @param {number} r
     * @param {number} g
     * @param {number} b
     * @returns {string}
     */
    function rgbToHex(r, g, b) {
        return '#' + [r, g, b].map(x => {
            const hex = x.toString(16);
            return hex.length === 1 ? '0' + hex : hex;
        }).join('');
    }

    /**
     * Adjust color brightness
     * @param {string} hex
     * @param {number} percent - Positive to lighten, negative to darken
     * @returns {string}
     */
    function adjustBrightness(hex, percent) {
        const rgb = hexToRgb(hex);
        if (!rgb) return hex;

        const adjust = (value) => {
            const adjusted = value + (value * percent / 100);
            return Math.min(255, Math.max(0, Math.round(adjusted)));
        };

        return rgbToHex(adjust(rgb.r), adjust(rgb.g), adjust(rgb.b));
    }

    /**
     * Generate a color with alpha
     * @param {string} hex
     * @param {number} alpha - 0 to 1
     * @returns {string}
     */
    function withAlpha(hex, alpha) {
        const rgb = hexToRgb(hex);
        if (!rgb) return hex;
        return `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, ${alpha})`;
    }

    /**
     * Check if the user prefers dark mode
     * @returns {boolean}
     */
    function prefersDarkMode() {
        return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    }

    /**
     * Check if the user prefers reduced motion
     * @returns {boolean}
     */
    function prefersReducedMotion() {
        return window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
    }

    // =========================================================================
    // Initialization
    // =========================================================================
    function init() {
        // Try to restore theme from localStorage
        try {
            const savedTheme = localStorage.getItem('nova3d-editor-theme');
            if (savedTheme && themes[savedTheme]) {
                currentTheme = savedTheme;
            } else if (prefersDarkMode()) {
                currentTheme = 'dark';
            }
        } catch (e) {
            // Use default
        }

        // Listen for system theme changes
        if (window.matchMedia) {
            window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e) => {
                if (!localStorage.getItem('nova3d-editor-theme')) {
                    setTheme(e.matches ? 'dark' : 'light');
                }
            });
        }

        // Apply initial theme
        applyTheme(themes[currentTheme]);
    }

    // Auto-init when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

    // =========================================================================
    // Public API
    // =========================================================================
    return {
        // Token collections
        colors,
        typography,
        spacing,
        borderRadius,
        shadows,
        transitions,
        zIndex,
        breakpoints,
        themes,

        // Theme management
        getTheme,
        setTheme,
        onThemeChange,
        setCustomTokens,
        resetCustomTokens,

        // Token accessors
        getCSSVar,
        setCSSVar,
        getColor,
        getSpacing,
        transition,

        // Utilities
        hexToRgb,
        rgbToHex,
        adjustBrightness,
        withAlpha,
        prefersDarkMode,
        prefersReducedMotion,

        // Re-init if needed
        init
    };
})();

// Export for module systems
if (typeof module !== 'undefined' && module.exports) {
    module.exports = DesignTokens;
}
