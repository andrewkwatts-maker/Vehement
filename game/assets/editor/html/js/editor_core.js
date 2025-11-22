/**
 * Nova3D Editor - Core Utilities
 * Bridge communication, event system, state management, and common utilities
 * @version 2.0
 */

const EditorCore = (function() {
    'use strict';

    // =========================================================================
    // Bridge Communication
    // =========================================================================

    const Bridge = {
        _callbacks: new Map(),
        _callbackId: 0,
        _ready: false,
        _queue: [],

        /**
         * Initialize bridge connection
         */
        init() {
            // Check if WebEditor exists (from C++ WebView)
            if (typeof WebEditor !== 'undefined') {
                this._ready = true;
                this._processQueue();
            } else {
                // Create mock for development
                window.WebEditor = {
                    invoke: (method, args, callback) => {
                        console.log('[Bridge Mock] invoke:', method, args);
                        if (callback) {
                            setTimeout(() => callback(null, { success: true }), 100);
                        }
                    },
                    navigate: (url, params) => {
                        console.log('[Bridge Mock] navigate:', url, params);
                    },
                    log: (msg) => console.log('[Bridge]', msg)
                };
                this._ready = true;
            }
        },

        /**
         * Invoke a method on the C++ side
         * @param {string} method - Method name
         * @param {Array} args - Arguments
         * @returns {Promise}
         */
        invoke(method, args = []) {
            return new Promise((resolve, reject) => {
                const callback = (err, result) => {
                    if (err) reject(err);
                    else resolve(result);
                };

                if (this._ready) {
                    WebEditor.invoke(method, args, callback);
                } else {
                    this._queue.push({ method, args, callback });
                }
            });
        },

        /**
         * Navigate to another editor page
         * @param {string} url - Page URL
         * @param {Object} params - Optional parameters
         */
        navigate(url, params = {}) {
            if (this._ready) {
                WebEditor.navigate(url, params);
            }
        },

        /**
         * Send log message to C++
         * @param {string} message
         * @param {string} level - 'info', 'warning', 'error'
         */
        log(message, level = 'info') {
            if (this._ready && WebEditor.log) {
                WebEditor.log(JSON.stringify({ message, level, timestamp: Date.now() }));
            }
            console[level === 'error' ? 'error' : level === 'warning' ? 'warn' : 'log'](`[Editor] ${message}`);
        },

        _processQueue() {
            while (this._queue.length > 0) {
                const { method, args, callback } = this._queue.shift();
                WebEditor.invoke(method, args, callback);
            }
        }
    };

    // =========================================================================
    // Event System
    // =========================================================================

    const Events = {
        _listeners: new Map(),

        /**
         * Subscribe to an event
         * @param {string} event - Event name
         * @param {Function} callback
         * @returns {Function} Unsubscribe function
         */
        on(event, callback) {
            if (!this._listeners.has(event)) {
                this._listeners.set(event, new Set());
            }
            this._listeners.get(event).add(callback);

            return () => this.off(event, callback);
        },

        /**
         * Subscribe to an event once
         * @param {string} event
         * @param {Function} callback
         */
        once(event, callback) {
            const wrapper = (...args) => {
                this.off(event, wrapper);
                callback(...args);
            };
            this.on(event, wrapper);
        },

        /**
         * Unsubscribe from an event
         * @param {string} event
         * @param {Function} callback
         */
        off(event, callback) {
            if (this._listeners.has(event)) {
                this._listeners.get(event).delete(callback);
            }
        },

        /**
         * Emit an event
         * @param {string} event
         * @param {*} data
         */
        emit(event, data) {
            if (this._listeners.has(event)) {
                this._listeners.get(event).forEach(callback => {
                    try {
                        callback(data);
                    } catch (e) {
                        console.error(`Error in event handler for "${event}":`, e);
                    }
                });
            }
        },

        /**
         * Clear all listeners for an event
         * @param {string} event
         */
        clear(event) {
            if (event) {
                this._listeners.delete(event);
            } else {
                this._listeners.clear();
            }
        }
    };

    // =========================================================================
    // State Management
    // =========================================================================

    const State = {
        _state: {},
        _subscribers: new Map(),

        /**
         * Get state value
         * @param {string} path - Dot-notation path
         * @returns {*}
         */
        get(path) {
            if (!path) return this._state;
            return path.split('.').reduce((obj, key) =>
                obj && obj[key] !== undefined ? obj[key] : undefined, this._state);
        },

        /**
         * Set state value
         * @param {string} path - Dot-notation path
         * @param {*} value
         */
        set(path, value) {
            const keys = path.split('.');
            const lastKey = keys.pop();
            const target = keys.reduce((obj, key) => {
                if (!obj[key]) obj[key] = {};
                return obj[key];
            }, this._state);

            const oldValue = target[lastKey];
            target[lastKey] = value;

            // Notify subscribers
            this._notifySubscribers(path, value, oldValue);
            Events.emit('state:change', { path, value, oldValue });
        },

        /**
         * Subscribe to state changes
         * @param {string} path - Path to watch (or '*' for all)
         * @param {Function} callback
         * @returns {Function} Unsubscribe function
         */
        subscribe(path, callback) {
            if (!this._subscribers.has(path)) {
                this._subscribers.set(path, new Set());
            }
            this._subscribers.get(path).add(callback);

            return () => {
                this._subscribers.get(path).delete(callback);
            };
        },

        /**
         * Update multiple state values
         * @param {Object} updates
         */
        update(updates) {
            Object.entries(updates).forEach(([path, value]) => {
                this.set(path, value);
            });
        },

        /**
         * Reset state to initial values
         * @param {Object} initialState
         */
        reset(initialState = {}) {
            this._state = initialState;
            Events.emit('state:reset', this._state);
        },

        _notifySubscribers(path, value, oldValue) {
            // Exact path subscribers
            if (this._subscribers.has(path)) {
                this._subscribers.get(path).forEach(cb => cb(value, oldValue, path));
            }

            // Wildcard subscribers
            if (this._subscribers.has('*')) {
                this._subscribers.get('*').forEach(cb => cb(value, oldValue, path));
            }

            // Parent path subscribers
            const parts = path.split('.');
            for (let i = parts.length - 1; i > 0; i--) {
                const parentPath = parts.slice(0, i).join('.');
                if (this._subscribers.has(parentPath + '.*')) {
                    this._subscribers.get(parentPath + '.*').forEach(cb => cb(value, oldValue, path));
                }
            }
        }
    };

    // =========================================================================
    // Keyboard Shortcuts
    // =========================================================================

    const Shortcuts = {
        _shortcuts: new Map(),
        _enabled: true,

        /**
         * Register a keyboard shortcut
         * @param {string} combo - Key combination (e.g., 'ctrl+s', 'ctrl+shift+z')
         * @param {Function} callback
         * @param {Object} options
         */
        register(combo, callback, options = {}) {
            const normalizedCombo = this._normalizeCombo(combo);
            this._shortcuts.set(normalizedCombo, {
                callback,
                description: options.description || '',
                global: options.global || false,
                preventDefault: options.preventDefault !== false
            });
        },

        /**
         * Unregister a shortcut
         * @param {string} combo
         */
        unregister(combo) {
            const normalizedCombo = this._normalizeCombo(combo);
            this._shortcuts.delete(normalizedCombo);
        },

        /**
         * Enable/disable shortcuts
         * @param {boolean} enabled
         */
        setEnabled(enabled) {
            this._enabled = enabled;
        },

        /**
         * Get all registered shortcuts
         * @returns {Array}
         */
        getAll() {
            return Array.from(this._shortcuts.entries()).map(([combo, data]) => ({
                combo,
                description: data.description
            }));
        },

        _init() {
            document.addEventListener('keydown', (e) => this._handleKeyDown(e));
        },

        _handleKeyDown(e) {
            if (!this._enabled) return;

            // Don't trigger shortcuts when typing in inputs
            if (!e.target.closest('input, textarea, [contenteditable]') || e.ctrlKey || e.metaKey) {
                const combo = this._eventToCombo(e);
                const shortcut = this._shortcuts.get(combo);

                if (shortcut) {
                    if (shortcut.preventDefault) {
                        e.preventDefault();
                    }
                    shortcut.callback(e);
                }
            }
        },

        _normalizeCombo(combo) {
            return combo.toLowerCase()
                .replace(/command|cmd/g, 'meta')
                .split('+')
                .sort()
                .join('+');
        },

        _eventToCombo(e) {
            const parts = [];
            if (e.ctrlKey) parts.push('ctrl');
            if (e.altKey) parts.push('alt');
            if (e.shiftKey) parts.push('shift');
            if (e.metaKey) parts.push('meta');

            const key = e.key.toLowerCase();
            if (!['control', 'alt', 'shift', 'meta'].includes(key)) {
                parts.push(key);
            }

            return parts.sort().join('+');
        }
    };

    // =========================================================================
    // Drag & Drop Utilities
    // =========================================================================

    const DragDrop = {
        _currentDrag: null,

        /**
         * Make an element draggable
         * @param {Element} element
         * @param {Object} options
         */
        makeDraggable(element, options = {}) {
            element.draggable = true;

            element.addEventListener('dragstart', (e) => {
                this._currentDrag = {
                    element,
                    data: options.getData ? options.getData() : null,
                    type: options.type || 'default'
                };

                e.dataTransfer.effectAllowed = options.effect || 'move';
                e.dataTransfer.setData('text/plain', JSON.stringify(this._currentDrag.data));

                if (options.onDragStart) {
                    options.onDragStart(e, this._currentDrag);
                }

                element.classList.add('dragging');
            });

            element.addEventListener('dragend', (e) => {
                element.classList.remove('dragging');
                if (options.onDragEnd) {
                    options.onDragEnd(e);
                }
                this._currentDrag = null;
            });
        },

        /**
         * Make an element a drop target
         * @param {Element} element
         * @param {Object} options
         */
        makeDropTarget(element, options = {}) {
            element.addEventListener('dragover', (e) => {
                if (options.canDrop && !options.canDrop(this._currentDrag)) {
                    return;
                }

                e.preventDefault();
                e.dataTransfer.dropEffect = options.effect || 'move';
                element.classList.add('drag-over');

                if (options.onDragOver) {
                    options.onDragOver(e, this._currentDrag);
                }
            });

            element.addEventListener('dragleave', (e) => {
                element.classList.remove('drag-over');
                if (options.onDragLeave) {
                    options.onDragLeave(e);
                }
            });

            element.addEventListener('drop', (e) => {
                e.preventDefault();
                element.classList.remove('drag-over');

                if (options.onDrop) {
                    options.onDrop(e, this._currentDrag);
                }
            });
        },

        /**
         * Get current drag data
         */
        getCurrentDrag() {
            return this._currentDrag;
        }
    };

    // =========================================================================
    // Context Menu System
    // =========================================================================

    const ContextMenu = {
        _menu: null,
        _visible: false,

        /**
         * Show context menu
         * @param {number} x - X position
         * @param {number} y - Y position
         * @param {Array} items - Menu items
         */
        show(x, y, items) {
            this.hide();

            this._menu = document.createElement('div');
            this._menu.className = 'context-menu visible';
            this._menu.style.cssText = `left:${x}px;top:${y}px;`;

            items.forEach(item => {
                if (item.separator) {
                    const sep = document.createElement('div');
                    sep.className = 'menu-separator';
                    this._menu.appendChild(sep);
                    return;
                }

                const menuItem = document.createElement('div');
                menuItem.className = 'menu-item';
                if (item.disabled) menuItem.classList.add('disabled');
                if (item.danger) menuItem.classList.add('danger');

                menuItem.innerHTML = `
                    ${item.icon ? `<span class="menu-item-icon">${item.icon}</span>` : ''}
                    <span class="menu-item-label">${item.label}</span>
                    ${item.shortcut ? `<span class="menu-item-shortcut">${item.shortcut}</span>` : ''}
                `;

                if (item.onClick && !item.disabled) {
                    menuItem.addEventListener('click', () => {
                        this.hide();
                        item.onClick();
                    });
                }

                this._menu.appendChild(menuItem);
            });

            document.body.appendChild(this._menu);
            this._visible = true;

            // Adjust position if off screen
            const rect = this._menu.getBoundingClientRect();
            if (rect.right > window.innerWidth) {
                this._menu.style.left = `${x - rect.width}px`;
            }
            if (rect.bottom > window.innerHeight) {
                this._menu.style.top = `${y - rect.height}px`;
            }

            // Close on click outside
            setTimeout(() => {
                document.addEventListener('click', this._closeHandler);
                document.addEventListener('contextmenu', this._closeHandler);
            }, 0);
        },

        /**
         * Hide context menu
         */
        hide() {
            if (this._menu) {
                this._menu.remove();
                this._menu = null;
                this._visible = false;
                document.removeEventListener('click', this._closeHandler);
                document.removeEventListener('contextmenu', this._closeHandler);
            }
        },

        _closeHandler: function(e) {
            if (!ContextMenu._menu?.contains(e.target)) {
                ContextMenu.hide();
            }
        },

        isVisible() {
            return this._visible;
        }
    };

    // =========================================================================
    // Notification System
    // =========================================================================

    const Notifications = {
        _container: null,

        _getContainer() {
            if (!this._container) {
                this._container = document.createElement('div');
                this._container.className = 'toast-container';
                document.body.appendChild(this._container);
            }
            return this._container;
        },

        /**
         * Show a notification
         * @param {Object} options
         * @returns {HTMLElement}
         */
        show(options = {}) {
            const {
                title = '',
                message = '',
                type = 'info',
                duration = 5000,
                closable = true,
                actions = []
            } = options;

            const toast = document.createElement('div');
            toast.className = `toast toast-${type}`;

            const icons = {
                success: '&#x2714;',
                warning: '&#x26A0;',
                error: '&#x2718;',
                info: '&#x2139;'
            };

            toast.innerHTML = `
                <span class="toast-icon">${icons[type] || icons.info}</span>
                <div class="toast-content">
                    ${title ? `<div class="toast-title">${title}</div>` : ''}
                    <div class="toast-message">${message}</div>
                    ${actions.length > 0 ? `
                        <div class="toast-actions">
                            ${actions.map((a, i) => `<button class="btn btn-sm" data-action="${i}">${a.label}</button>`).join('')}
                        </div>
                    ` : ''}
                </div>
                ${closable ? '<button class="toast-close">&times;</button>' : ''}
            `;

            // Handle actions
            actions.forEach((action, i) => {
                const btn = toast.querySelector(`[data-action="${i}"]`);
                if (btn) {
                    btn.addEventListener('click', () => {
                        action.onClick();
                        this.dismiss(toast);
                    });
                }
            });

            // Handle close button
            if (closable) {
                toast.querySelector('.toast-close').addEventListener('click', () => {
                    this.dismiss(toast);
                });
            }

            this._getContainer().appendChild(toast);

            // Auto dismiss
            if (duration > 0) {
                setTimeout(() => this.dismiss(toast), duration);
            }

            return toast;
        },

        /**
         * Dismiss a notification
         * @param {HTMLElement} toast
         */
        dismiss(toast) {
            toast.style.animation = 'fade-out 0.2s ease-out forwards';
            setTimeout(() => toast.remove(), 200);
        },

        // Convenience methods
        success(message, title = '') {
            return this.show({ message, title, type: 'success' });
        },

        warning(message, title = '') {
            return this.show({ message, title, type: 'warning' });
        },

        error(message, title = '') {
            return this.show({ message, title, type: 'error', duration: 8000 });
        },

        info(message, title = '') {
            return this.show({ message, title, type: 'info' });
        }
    };

    // =========================================================================
    // Undo/Redo Manager
    // =========================================================================

    const UndoRedo = {
        _history: [],
        _position: -1,
        _maxHistory: 100,
        _grouping: false,
        _currentGroup: null,

        /**
         * Push an action to the history
         * @param {Object} action - { undo: Function, redo: Function, description: string }
         */
        push(action) {
            // Clear any redo history
            if (this._position < this._history.length - 1) {
                this._history = this._history.slice(0, this._position + 1);
            }

            if (this._grouping && this._currentGroup) {
                this._currentGroup.actions.push(action);
            } else {
                this._history.push(action);
                this._position++;

                // Trim history if too long
                if (this._history.length > this._maxHistory) {
                    this._history.shift();
                    this._position--;
                }
            }

            Events.emit('history:change', this.getState());
        },

        /**
         * Undo the last action
         */
        undo() {
            if (!this.canUndo()) return false;

            const action = this._history[this._position];
            this._position--;

            if (action.actions) {
                // Grouped actions
                for (let i = action.actions.length - 1; i >= 0; i--) {
                    action.actions[i].undo();
                }
            } else {
                action.undo();
            }

            Events.emit('history:undo', action);
            Events.emit('history:change', this.getState());
            return true;
        },

        /**
         * Redo the last undone action
         */
        redo() {
            if (!this.canRedo()) return false;

            this._position++;
            const action = this._history[this._position];

            if (action.actions) {
                // Grouped actions
                action.actions.forEach(a => a.redo());
            } else {
                action.redo();
            }

            Events.emit('history:redo', action);
            Events.emit('history:change', this.getState());
            return true;
        },

        /**
         * Start grouping actions
         * @param {string} description
         */
        beginGroup(description = '') {
            this._grouping = true;
            this._currentGroup = { description, actions: [] };
        },

        /**
         * End grouping and push as single action
         */
        endGroup() {
            if (this._grouping && this._currentGroup && this._currentGroup.actions.length > 0) {
                const group = this._currentGroup;
                this._grouping = false;
                this._currentGroup = null;

                this._history.push({
                    description: group.description,
                    actions: group.actions,
                    undo: () => {
                        for (let i = group.actions.length - 1; i >= 0; i--) {
                            group.actions[i].undo();
                        }
                    },
                    redo: () => {
                        group.actions.forEach(a => a.redo());
                    }
                });
                this._position++;
            }

            this._grouping = false;
            this._currentGroup = null;
        },

        canUndo() {
            return this._position >= 0;
        },

        canRedo() {
            return this._position < this._history.length - 1;
        },

        getState() {
            return {
                canUndo: this.canUndo(),
                canRedo: this.canRedo(),
                undoDescription: this._position >= 0 ? this._history[this._position].description : '',
                redoDescription: this._position < this._history.length - 1 ? this._history[this._position + 1].description : ''
            };
        },

        clear() {
            this._history = [];
            this._position = -1;
            Events.emit('history:change', this.getState());
        }
    };

    // =========================================================================
    // Utility Functions
    // =========================================================================

    const Utils = {
        /**
         * Debounce a function
         */
        debounce(fn, delay) {
            let timeout;
            return function(...args) {
                clearTimeout(timeout);
                timeout = setTimeout(() => fn.apply(this, args), delay);
            };
        },

        /**
         * Throttle a function
         */
        throttle(fn, limit) {
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
         * Generate a unique ID
         */
        generateId(prefix = 'nova') {
            return `${prefix}_${Date.now().toString(36)}_${Math.random().toString(36).substr(2, 9)}`;
        },

        /**
         * Deep clone an object
         */
        deepClone(obj) {
            if (obj === null || typeof obj !== 'object') return obj;
            if (obj instanceof Date) return new Date(obj);
            if (obj instanceof Array) return obj.map(item => this.deepClone(item));
            if (obj instanceof Object) {
                const copy = {};
                Object.keys(obj).forEach(key => {
                    copy[key] = this.deepClone(obj[key]);
                });
                return copy;
            }
            return obj;
        },

        /**
         * Format a file size
         */
        formatFileSize(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        },

        /**
         * Format a duration
         */
        formatDuration(ms) {
            const seconds = Math.floor(ms / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);

            if (hours > 0) {
                return `${hours}h ${minutes % 60}m`;
            } else if (minutes > 0) {
                return `${minutes}m ${seconds % 60}s`;
            }
            return `${seconds}s`;
        },

        /**
         * Check if an element is visible in viewport
         */
        isInViewport(element) {
            const rect = element.getBoundingClientRect();
            return (
                rect.top >= 0 &&
                rect.left >= 0 &&
                rect.bottom <= window.innerHeight &&
                rect.right <= window.innerWidth
            );
        },

        /**
         * Wait for a condition to be true
         */
        waitFor(condition, timeout = 5000, interval = 100) {
            return new Promise((resolve, reject) => {
                const startTime = Date.now();
                const check = () => {
                    if (condition()) {
                        resolve();
                    } else if (Date.now() - startTime > timeout) {
                        reject(new Error('Timeout waiting for condition'));
                    } else {
                        setTimeout(check, interval);
                    }
                };
                check();
            });
        },

        /**
         * Load a script dynamically
         */
        loadScript(src) {
            return new Promise((resolve, reject) => {
                const script = document.createElement('script');
                script.src = src;
                script.onload = resolve;
                script.onerror = reject;
                document.head.appendChild(script);
            });
        },

        /**
         * Load CSS dynamically
         */
        loadCSS(href) {
            return new Promise((resolve, reject) => {
                const link = document.createElement('link');
                link.rel = 'stylesheet';
                link.href = href;
                link.onload = resolve;
                link.onerror = reject;
                document.head.appendChild(link);
            });
        }
    };

    // =========================================================================
    // Initialization
    // =========================================================================

    function init() {
        Bridge.init();
        Shortcuts._init();

        // Register default shortcuts
        Shortcuts.register('ctrl+z', () => UndoRedo.undo(), { description: 'Undo' });
        Shortcuts.register('ctrl+shift+z', () => UndoRedo.redo(), { description: 'Redo' });
        Shortcuts.register('ctrl+y', () => UndoRedo.redo(), { description: 'Redo' });

        Events.emit('editor:ready');
    }

    // Auto-init
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

    // =========================================================================
    // Public API
    // =========================================================================

    return {
        Bridge,
        Events,
        State,
        Shortcuts,
        DragDrop,
        ContextMenu,
        Notifications,
        UndoRedo,
        Utils,
        init
    };
})();

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorCore;
}

if (typeof window !== 'undefined') {
    window.EditorCore = EditorCore;
}
