/**
 * Nova3D/Vehement2 Web Editor Core JavaScript
 * Provides bridge to C++ and common utilities
 */

// ============================================================================
// WebEditor Bridge
// ============================================================================

window.WebEditor = window.WebEditor || {
    _callbacks: {},
    _callbackId: 0,
    _messageHandlers: {},
    _eventHandlers: {},

    /**
     * Initialize the bridge
     */
    init: function() {
        console.log('WebEditor bridge initializing...');
        this._setupMessageListener();
    },

    /**
     * Set up message listener for incoming C++ messages
     */
    _setupMessageListener: function() {
        window.addEventListener('webeditor-message', function(event) {
            var detail = event.detail;
            if (detail && detail.type) {
                WebEditor._handleIncomingMessage(detail.type, detail.payload);
            }
        });
    },

    /**
     * Post message to C++ backend
     */
    postMessage: function(type, payload) {
        var message = JSON.stringify({ type: type, payload: payload });

        // Try different bridge methods
        if (window.external && window.external.invoke) {
            // WebView2 (Windows)
            window.external.invoke(message);
        } else if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.webeditor) {
            // WKWebView (macOS)
            window.webkit.messageHandlers.webeditor.postMessage({ type: type, payload: payload });
        } else if (window.chrome && window.chrome.webview) {
            // Alternative WebView2
            window.chrome.webview.postMessage(message);
        } else {
            // Fallback: log to console
            console.log('[WebEditor] Message:', type, payload);
        }
    },

    /**
     * Invoke a C++ function and get result via callback
     */
    invoke: function(functionName, args, callback) {
        var id = ++this._callbackId;

        if (callback) {
            this._callbacks[id] = callback;
        }

        this.postMessage('invoke', {
            id: id,
            function: functionName,
            args: args || []
        });

        return id;
    },

    /**
     * Invoke with Promise
     */
    invokeAsync: function(functionName, args) {
        var self = this;
        return new Promise(function(resolve, reject) {
            self.invoke(functionName, args, function(error, result) {
                if (error) {
                    reject(error);
                } else {
                    resolve(result);
                }
            });
        });
    },

    /**
     * Called by C++ to deliver results
     */
    _handleResult: function(id, result, error) {
        var callback = this._callbacks[id];
        if (callback) {
            delete this._callbacks[id];
            callback(error, result);
        }
    },

    /**
     * Called by C++ to deliver messages
     */
    _handleMessage: function(type, payload) {
        this._handleIncomingMessage(type, payload);
    },

    /**
     * Handle incoming message
     */
    _handleIncomingMessage: function(type, payload) {
        // Check for registered handlers
        var handlers = this._messageHandlers[type];
        if (handlers) {
            handlers.forEach(function(handler) {
                try {
                    handler(payload);
                } catch (e) {
                    console.error('Message handler error:', e);
                }
            });
        }

        // Dispatch custom event
        var event = new CustomEvent('webeditor-' + type, { detail: payload });
        window.dispatchEvent(event);
    },

    /**
     * Subscribe to messages from C++
     */
    onMessage: function(type, handler) {
        if (!this._messageHandlers[type]) {
            this._messageHandlers[type] = [];
        }
        this._messageHandlers[type].push(handler);
    },

    /**
     * Unsubscribe from messages
     */
    offMessage: function(type, handler) {
        var handlers = this._messageHandlers[type];
        if (handlers) {
            var index = handlers.indexOf(handler);
            if (index > -1) {
                handlers.splice(index, 1);
            }
        }
    },

    /**
     * Emit event to C++
     */
    emit: function(eventName, data) {
        this.postMessage('event', {
            event: eventName,
            data: data || {}
        });
    },

    /**
     * Get URL query parameter
     */
    getQueryParam: function(name) {
        var params = new URLSearchParams(window.location.search);
        return params.get(name);
    },

    /**
     * Navigate to another editor page
     */
    navigate: function(page, params) {
        var url = page;
        if (params) {
            var queryString = Object.keys(params)
                .map(function(key) { return key + '=' + encodeURIComponent(params[key]); })
                .join('&');
            url += '?' + queryString;
        }
        window.location.href = url;
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Deep clone an object
 */
function deepClone(obj) {
    return JSON.parse(JSON.stringify(obj));
}

/**
 * Deep merge objects
 */
function deepMerge(target, source) {
    var output = Object.assign({}, target);
    if (isObject(target) && isObject(source)) {
        Object.keys(source).forEach(function(key) {
            if (isObject(source[key])) {
                if (!(key in target)) {
                    Object.assign(output, { [key]: source[key] });
                } else {
                    output[key] = deepMerge(target[key], source[key]);
                }
            } else {
                Object.assign(output, { [key]: source[key] });
            }
        });
    }
    return output;
}

function isObject(item) {
    return item && typeof item === 'object' && !Array.isArray(item);
}

/**
 * Get value at path in object
 */
function getPath(obj, path) {
    if (!path) return obj;
    var parts = path.split('.');
    var current = obj;
    for (var i = 0; i < parts.length; i++) {
        if (current === null || current === undefined) return undefined;
        current = current[parts[i]];
    }
    return current;
}

/**
 * Set value at path in object
 */
function setPath(obj, path, value) {
    var parts = path.split('.');
    var current = obj;
    for (var i = 0; i < parts.length - 1; i++) {
        var part = parts[i];
        if (current[part] === undefined || current[part] === null) {
            current[part] = {};
        }
        current = current[part];
    }
    current[parts[parts.length - 1]] = value;
}

/**
 * Delete value at path in object
 */
function deletePath(obj, path) {
    var parts = path.split('.');
    var current = obj;
    for (var i = 0; i < parts.length - 1; i++) {
        if (current === null || current === undefined) return;
        current = current[parts[i]];
    }
    if (current) {
        delete current[parts[parts.length - 1]];
    }
}

/**
 * Debounce function
 */
function debounce(func, wait) {
    var timeout;
    return function() {
        var context = this;
        var args = arguments;
        clearTimeout(timeout);
        timeout = setTimeout(function() {
            func.apply(context, args);
        }, wait);
    };
}

/**
 * Throttle function
 */
function throttle(func, limit) {
    var inThrottle;
    return function() {
        var context = this;
        var args = arguments;
        if (!inThrottle) {
            func.apply(context, args);
            inThrottle = true;
            setTimeout(function() { inThrottle = false; }, limit);
        }
    };
}

/**
 * Generate UUID
 */
function generateUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
        var r = Math.random() * 16 | 0;
        var v = c === 'x' ? r : (r & 0x3 | 0x8);
        return v.toString(16);
    });
}

/**
 * Format number with commas
 */
function formatNumber(num) {
    return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
}

/**
 * Format file size
 */
function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';
    var k = 1024;
    var sizes = ['Bytes', 'KB', 'MB', 'GB'];
    var i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

/**
 * Format duration in seconds
 */
function formatDuration(seconds) {
    if (seconds < 60) return seconds + 's';
    if (seconds < 3600) return Math.floor(seconds / 60) + 'm ' + (seconds % 60) + 's';
    var hours = Math.floor(seconds / 3600);
    var mins = Math.floor((seconds % 3600) / 60);
    return hours + 'h ' + mins + 'm';
}

// ============================================================================
// DOM Utilities
// ============================================================================

/**
 * Query selector shorthand
 */
function $(selector) {
    return document.querySelector(selector);
}

function $$(selector) {
    return document.querySelectorAll(selector);
}

/**
 * Create element with attributes
 */
function createElement(tag, attrs, children) {
    var el = document.createElement(tag);
    if (attrs) {
        Object.keys(attrs).forEach(function(key) {
            if (key === 'className') {
                el.className = attrs[key];
            } else if (key === 'style' && typeof attrs[key] === 'object') {
                Object.assign(el.style, attrs[key]);
            } else if (key.startsWith('on')) {
                el.addEventListener(key.substr(2).toLowerCase(), attrs[key]);
            } else {
                el.setAttribute(key, attrs[key]);
            }
        });
    }
    if (children) {
        if (typeof children === 'string') {
            el.textContent = children;
        } else if (Array.isArray(children)) {
            children.forEach(function(child) {
                if (typeof child === 'string') {
                    el.appendChild(document.createTextNode(child));
                } else if (child) {
                    el.appendChild(child);
                }
            });
        }
    }
    return el;
}

/**
 * Add class to element
 */
function addClass(el, className) {
    if (el && className) {
        el.classList.add(className);
    }
}

/**
 * Remove class from element
 */
function removeClass(el, className) {
    if (el && className) {
        el.classList.remove(className);
    }
}

/**
 * Toggle class on element
 */
function toggleClass(el, className, force) {
    if (el && className) {
        el.classList.toggle(className, force);
    }
}

/**
 * Check if element has class
 */
function hasClass(el, className) {
    return el && el.classList.contains(className);
}

// ============================================================================
// Event Handling
// ============================================================================

/**
 * Add event listener
 */
function on(el, event, handler, options) {
    if (typeof el === 'string') {
        el = $(el);
    }
    if (el) {
        el.addEventListener(event, handler, options);
    }
}

/**
 * Remove event listener
 */
function off(el, event, handler, options) {
    if (typeof el === 'string') {
        el = $(el);
    }
    if (el) {
        el.removeEventListener(event, handler, options);
    }
}

/**
 * Add one-time event listener
 */
function once(el, event, handler) {
    if (typeof el === 'string') {
        el = $(el);
    }
    if (el) {
        el.addEventListener(event, handler, { once: true });
    }
}

/**
 * Delegate event listener
 */
function delegate(el, event, selector, handler) {
    if (typeof el === 'string') {
        el = $(el);
    }
    if (el) {
        el.addEventListener(event, function(e) {
            var target = e.target.closest(selector);
            if (target && el.contains(target)) {
                handler.call(target, e);
            }
        });
    }
}

// ============================================================================
// Keyboard Shortcuts
// ============================================================================

var _shortcuts = {};

/**
 * Register keyboard shortcut
 */
function registerShortcut(keys, callback, description) {
    _shortcuts[keys.toLowerCase()] = {
        callback: callback,
        description: description || ''
    };
}

/**
 * Handle keydown for shortcuts
 */
document.addEventListener('keydown', function(e) {
    // Build key string
    var parts = [];
    if (e.ctrlKey || e.metaKey) parts.push('ctrl');
    if (e.altKey) parts.push('alt');
    if (e.shiftKey) parts.push('shift');
    parts.push(e.key.toLowerCase());

    var keyString = parts.join('+');
    var shortcut = _shortcuts[keyString];

    if (shortcut) {
        e.preventDefault();
        shortcut.callback(e);
    }
});

// ============================================================================
// Common Editor Functions
// ============================================================================

var _isDirty = false;

/**
 * Mark document as dirty
 */
function setDirty(dirty) {
    _isDirty = dirty;
    var indicator = document.getElementById('dirty-indicator');
    if (indicator) {
        indicator.style.display = dirty ? 'inline' : 'none';
    }
}

/**
 * Check if document is dirty
 */
function isDirty() {
    return _isDirty;
}

/**
 * Go back to content browser
 */
function goBack() {
    if (_isDirty) {
        if (!confirm('You have unsaved changes. Discard them?')) {
            return;
        }
    }
    WebEditor.navigate('content_browser.html');
}

/**
 * Toggle section collapse
 */
function toggleSection(header) {
    var section = header.parentElement;
    toggleClass(section, 'collapsed');
}

/**
 * Expand all sections
 */
function expandAllSections() {
    $$('.section.collapsed').forEach(function(section) {
        removeClass(section, 'collapsed');
    });
}

/**
 * Collapse all sections
 */
function collapseAllSections() {
    $$('.section').forEach(function(section) {
        addClass(section, 'collapsed');
    });
}

// ============================================================================
// Context Menu
// ============================================================================

var _contextMenu = null;
var _contextTarget = null;

/**
 * Show context menu
 */
function showContextMenu(x, y, items, target) {
    hideContextMenu();

    _contextTarget = target;
    _contextMenu = createElement('div', { className: 'context-menu' });

    items.forEach(function(item) {
        if (item.separator) {
            _contextMenu.appendChild(createElement('div', { className: 'menu-separator' }));
        } else {
            var menuItem = createElement('div', {
                className: 'menu-item' + (item.danger ? ' danger' : ''),
                onClick: function() {
                    hideContextMenu();
                    if (item.onClick) {
                        item.onClick(_contextTarget);
                    }
                }
            }, item.label);
            _contextMenu.appendChild(menuItem);
        }
    });

    _contextMenu.style.left = x + 'px';
    _contextMenu.style.top = y + 'px';
    document.body.appendChild(_contextMenu);

    // Adjust if out of viewport
    var rect = _contextMenu.getBoundingClientRect();
    if (rect.right > window.innerWidth) {
        _contextMenu.style.left = (x - rect.width) + 'px';
    }
    if (rect.bottom > window.innerHeight) {
        _contextMenu.style.top = (y - rect.height) + 'px';
    }
}

/**
 * Hide context menu
 */
function hideContextMenu() {
    if (_contextMenu) {
        _contextMenu.remove();
        _contextMenu = null;
        _contextTarget = null;
    }
}

// Hide context menu on click outside
document.addEventListener('click', hideContextMenu);
document.addEventListener('contextmenu', function(e) {
    // Allow native context menu on input elements
    if (e.target.tagName !== 'INPUT' && e.target.tagName !== 'TEXTAREA') {
        e.preventDefault();
    }
});

// ============================================================================
// Default Keyboard Shortcuts
// ============================================================================

registerShortcut('ctrl+s', function() {
    if (typeof save === 'function') {
        save();
    }
}, 'Save');

registerShortcut('ctrl+z', function() {
    if (typeof undo === 'function') {
        undo();
    }
}, 'Undo');

registerShortcut('ctrl+y', function() {
    if (typeof redo === 'function') {
        redo();
    }
}, 'Redo');

registerShortcut('ctrl+shift+z', function() {
    if (typeof redo === 'function') {
        redo();
    }
}, 'Redo');

// ============================================================================
// Initialize
// ============================================================================

document.addEventListener('DOMContentLoaded', function() {
    WebEditor.init();
});

// Warn before leaving with unsaved changes
window.addEventListener('beforeunload', function(e) {
    if (_isDirty) {
        e.preventDefault();
        e.returnValue = '';
        return '';
    }
});
