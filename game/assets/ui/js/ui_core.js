/**
 * UI Core - Core UI utilities and engine communication
 */

(function(global) {
    'use strict';

    // ===== Engine Bridge =====
    const Engine = {
        _callbacks: new Map(),
        _callbackId: 0,
        _ready: false,
        _pendingCalls: [],

        /**
         * Initialize engine connection
         */
        init: function() {
            // Check if native engine interface exists
            if (typeof window.NativeEngine !== 'undefined') {
                this._ready = true;
                this._processPendingCalls();
            } else {
                // Wait for engine to inject interface
                window.addEventListener('engineReady', () => {
                    this._ready = true;
                    this._processPendingCalls();
                });
            }

            // Setup message handler for engine responses
            window.addEventListener('message', (e) => this._handleMessage(e));
        },

        /**
         * Call a C++ function from JavaScript
         */
        call: function(functionName, args, callback) {
            const callData = {
                function: functionName,
                args: args || {},
                callbackId: null
            };

            if (callback) {
                callData.callbackId = ++this._callbackId;
                this._callbacks.set(callData.callbackId, callback);

                // Timeout for callbacks
                setTimeout(() => {
                    if (this._callbacks.has(callData.callbackId)) {
                        this._callbacks.delete(callData.callbackId);
                        console.warn(`Engine call timeout: ${functionName}`);
                    }
                }, 10000);
            }

            if (this._ready) {
                this._sendToEngine(callData);
            } else {
                this._pendingCalls.push(callData);
            }
        },

        /**
         * Register a function that can be called from C++
         */
        expose: function(name, handler) {
            window.UI = window.UI || {};
            window.UI[name] = handler;
        },

        _sendToEngine: function(data) {
            if (typeof window.NativeEngine !== 'undefined') {
                window.NativeEngine.postMessage(JSON.stringify(data));
            } else {
                console.log('[Engine Call]', data.function, data.args);
            }
        },

        _handleMessage: function(event) {
            try {
                const data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;

                if (data.callbackId && this._callbacks.has(data.callbackId)) {
                    const callback = this._callbacks.get(data.callbackId);
                    this._callbacks.delete(data.callbackId);
                    callback(data.result, data.error);
                }

                if (data.event && window.UI && window.UI[data.event]) {
                    window.UI[data.event](data.data);
                }
            } catch (e) {
                console.error('Engine message handling error:', e);
            }
        },

        _processPendingCalls: function() {
            this._pendingCalls.forEach(call => this._sendToEngine(call));
            this._pendingCalls = [];
        }
    };

    // ===== DOM Utilities =====
    const DOM = {
        /**
         * Query selector shorthand
         */
        $: function(selector, context) {
            return (context || document).querySelector(selector);
        },

        /**
         * Query selector all shorthand
         */
        $$: function(selector, context) {
            return Array.from((context || document).querySelectorAll(selector));
        },

        /**
         * Create element with attributes and children
         */
        create: function(tag, attrs, children) {
            const el = document.createElement(tag);

            if (attrs) {
                Object.entries(attrs).forEach(([key, value]) => {
                    if (key === 'class') {
                        el.className = value;
                    } else if (key === 'style' && typeof value === 'object') {
                        Object.assign(el.style, value);
                    } else if (key === 'data' && typeof value === 'object') {
                        Object.entries(value).forEach(([k, v]) => el.dataset[k] = v);
                    } else if (key.startsWith('on') && typeof value === 'function') {
                        el.addEventListener(key.slice(2).toLowerCase(), value);
                    } else {
                        el.setAttribute(key, value);
                    }
                });
            }

            if (children) {
                if (Array.isArray(children)) {
                    children.forEach(child => {
                        if (typeof child === 'string') {
                            el.appendChild(document.createTextNode(child));
                        } else if (child instanceof Node) {
                            el.appendChild(child);
                        }
                    });
                } else if (typeof children === 'string') {
                    el.textContent = children;
                } else if (children instanceof Node) {
                    el.appendChild(children);
                }
            }

            return el;
        },

        /**
         * Remove element safely
         */
        remove: function(el) {
            if (el && el.parentNode) {
                el.parentNode.removeChild(el);
            }
        },

        /**
         * Empty element contents
         */
        empty: function(el) {
            while (el.firstChild) {
                el.removeChild(el.firstChild);
            }
        },

        /**
         * Add class with optional condition
         */
        addClass: function(el, className, condition = true) {
            if (condition) {
                el.classList.add(className);
            }
            return this;
        },

        /**
         * Remove class
         */
        removeClass: function(el, className) {
            el.classList.remove(className);
            return this;
        },

        /**
         * Toggle class
         */
        toggleClass: function(el, className, force) {
            el.classList.toggle(className, force);
            return this;
        },

        /**
         * Check if element has class
         */
        hasClass: function(el, className) {
            return el.classList.contains(className);
        },

        /**
         * Show element
         */
        show: function(el, display = 'block') {
            el.style.display = display;
        },

        /**
         * Hide element
         */
        hide: function(el) {
            el.style.display = 'none';
        },

        /**
         * Get element position relative to viewport
         */
        getPosition: function(el) {
            const rect = el.getBoundingClientRect();
            return {
                top: rect.top,
                left: rect.left,
                right: rect.right,
                bottom: rect.bottom,
                width: rect.width,
                height: rect.height
            };
        }
    };

    // ===== Event Utilities =====
    const Events = {
        _listeners: new Map(),

        /**
         * Add event listener with namespace support
         */
        on: function(target, event, handler, options) {
            const el = typeof target === 'string' ? DOM.$(target) : target;
            if (!el) return;

            const [eventName, namespace] = event.split('.');

            el.addEventListener(eventName, handler, options);

            // Store for later removal
            if (!this._listeners.has(el)) {
                this._listeners.set(el, []);
            }
            this._listeners.get(el).push({ eventName, namespace, handler, options });
        },

        /**
         * Remove event listener(s)
         */
        off: function(target, event, handler) {
            const el = typeof target === 'string' ? DOM.$(target) : target;
            if (!el) return;

            const [eventName, namespace] = event ? event.split('.') : [null, null];
            const listeners = this._listeners.get(el) || [];

            listeners.forEach((listener, index) => {
                if ((!eventName || listener.eventName === eventName) &&
                    (!namespace || listener.namespace === namespace) &&
                    (!handler || listener.handler === handler)) {
                    el.removeEventListener(listener.eventName, listener.handler, listener.options);
                    listeners.splice(index, 1);
                }
            });
        },

        /**
         * Trigger custom event
         */
        trigger: function(target, event, detail) {
            const el = typeof target === 'string' ? DOM.$(target) : target;
            if (!el) return;

            el.dispatchEvent(new CustomEvent(event, { detail, bubbles: true }));
        },

        /**
         * One-time event listener
         */
        once: function(target, event, handler) {
            this.on(target, event, function onceHandler(e) {
                handler(e);
                Events.off(target, event, onceHandler);
            });
        },

        /**
         * Delegate event handling
         */
        delegate: function(target, event, selector, handler) {
            this.on(target, event, function(e) {
                const delegateTarget = e.target.closest(selector);
                if (delegateTarget && target.contains(delegateTarget)) {
                    handler.call(delegateTarget, e);
                }
            });
        }
    };

    // ===== Utility Functions =====
    const Utils = {
        /**
         * Generate unique ID
         */
        uid: function(prefix = 'uid') {
            return `${prefix}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
        },

        /**
         * Debounce function
         */
        debounce: function(fn, delay) {
            let timeout;
            return function(...args) {
                clearTimeout(timeout);
                timeout = setTimeout(() => fn.apply(this, args), delay);
            };
        },

        /**
         * Throttle function
         */
        throttle: function(fn, limit) {
            let inThrottle;
            return function(...args) {
                if (!inThrottle) {
                    fn.apply(this, args);
                    inThrottle = true;
                    setTimeout(() => inThrottle = false, limit);
                }
            };
        },

        /**
         * Deep clone object
         */
        clone: function(obj) {
            return JSON.parse(JSON.stringify(obj));
        },

        /**
         * Deep merge objects
         */
        merge: function(target, ...sources) {
            sources.forEach(source => {
                Object.keys(source).forEach(key => {
                    if (source[key] && typeof source[key] === 'object' && !Array.isArray(source[key])) {
                        target[key] = target[key] || {};
                        this.merge(target[key], source[key]);
                    } else {
                        target[key] = source[key];
                    }
                });
            });
            return target;
        },

        /**
         * Format number with commas
         */
        formatNumber: function(num) {
            return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
        },

        /**
         * Format time duration
         */
        formatDuration: function(seconds) {
            if (seconds < 60) return `${Math.floor(seconds)}s`;
            if (seconds < 3600) return `${Math.floor(seconds / 60)}m ${Math.floor(seconds % 60)}s`;
            return `${Math.floor(seconds / 3600)}h ${Math.floor((seconds % 3600) / 60)}m`;
        },

        /**
         * Clamp value between min and max
         */
        clamp: function(value, min, max) {
            return Math.min(Math.max(value, min), max);
        },

        /**
         * Linear interpolation
         */
        lerp: function(start, end, t) {
            return start + (end - start) * t;
        },

        /**
         * Map value from one range to another
         */
        mapRange: function(value, inMin, inMax, outMin, outMax) {
            return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
        },

        /**
         * Parse color string to components
         */
        parseColor: function(color) {
            const canvas = document.createElement('canvas');
            canvas.width = canvas.height = 1;
            const ctx = canvas.getContext('2d');
            ctx.fillStyle = color;
            ctx.fillRect(0, 0, 1, 1);
            const data = ctx.getImageData(0, 0, 1, 1).data;
            return { r: data[0], g: data[1], b: data[2], a: data[3] / 255 };
        },

        /**
         * Check if device is touch-capable
         */
        isTouchDevice: function() {
            return 'ontouchstart' in window || navigator.maxTouchPoints > 0;
        },

        /**
         * Check if device is mobile
         */
        isMobile: function() {
            return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
        },

        /**
         * Get viewport dimensions
         */
        getViewport: function() {
            return {
                width: window.innerWidth,
                height: window.innerHeight
            };
        },

        /**
         * Escape HTML
         */
        escapeHtml: function(str) {
            const div = document.createElement('div');
            div.textContent = str;
            return div.innerHTML;
        }
    };

    // ===== Storage =====
    const Storage = {
        _prefix: 'ui_',

        /**
         * Set prefix for storage keys
         */
        setPrefix: function(prefix) {
            this._prefix = prefix;
        },

        /**
         * Save to localStorage
         */
        set: function(key, value) {
            try {
                localStorage.setItem(this._prefix + key, JSON.stringify(value));
                return true;
            } catch (e) {
                console.warn('Storage set failed:', e);
                return false;
            }
        },

        /**
         * Load from localStorage
         */
        get: function(key, defaultValue) {
            try {
                const value = localStorage.getItem(this._prefix + key);
                return value !== null ? JSON.parse(value) : defaultValue;
            } catch (e) {
                return defaultValue;
            }
        },

        /**
         * Remove from localStorage
         */
        remove: function(key) {
            localStorage.removeItem(this._prefix + key);
        },

        /**
         * Clear all prefixed items
         */
        clear: function() {
            Object.keys(localStorage)
                .filter(key => key.startsWith(this._prefix))
                .forEach(key => localStorage.removeItem(key));
        }
    };

    // ===== Template Engine =====
    const Template = {
        _cache: new Map(),

        /**
         * Compile and cache template
         */
        compile: function(templateId) {
            if (this._cache.has(templateId)) {
                return this._cache.get(templateId);
            }

            const template = DOM.$(`#${templateId}`);
            if (!template) {
                console.error(`Template not found: ${templateId}`);
                return null;
            }

            const compiled = this._compileString(template.innerHTML);
            this._cache.set(templateId, compiled);
            return compiled;
        },

        /**
         * Compile template string
         */
        _compileString: function(str) {
            return function(data) {
                return str.replace(/\{\{(\w+(?:\.\w+)*)\}\}/g, (match, path) => {
                    const value = path.split('.').reduce((obj, key) => obj && obj[key], data);
                    return value !== undefined ? value : '';
                });
            };
        },

        /**
         * Render template with data
         */
        render: function(templateId, data) {
            const compiled = this.compile(templateId);
            return compiled ? compiled(data) : '';
        },

        /**
         * Render and insert into element
         */
        renderTo: function(target, templateId, data) {
            const el = typeof target === 'string' ? DOM.$(target) : target;
            if (el) {
                el.innerHTML = this.render(templateId, data);
            }
        }
    };

    // ===== Export =====
    global.UICore = {
        Engine,
        DOM,
        Events,
        Utils,
        Storage,
        Template,

        // Shortcuts
        $: DOM.$,
        $$: DOM.$$,
        create: DOM.create
    };

    // Initialize engine connection
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => Engine.init());
    } else {
        Engine.init();
    }

    // Expose Engine globally
    global.Engine = Engine;

})(window);
