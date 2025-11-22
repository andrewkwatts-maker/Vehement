/**
 * UI Data Binding - Reactive data binding between JavaScript and game state
 */

(function(global) {
    'use strict';

    // ===== Observable Model =====
    class ObservableModel {
        constructor(initialData = {}) {
            this._data = {};
            this._watchers = new Map();
            this._computed = new Map();
            this._batchUpdates = false;
            this._pendingUpdates = new Set();

            // Initialize with data
            Object.entries(initialData).forEach(([key, value]) => {
                this.set(key, value);
            });
        }

        /**
         * Get value at path
         */
        get(path) {
            // Check computed first
            if (this._computed.has(path)) {
                return this._computed.get(path).getter();
            }

            const keys = path.split('.');
            let value = this._data;

            for (const key of keys) {
                if (value === null || value === undefined) return undefined;
                value = value[key];
            }

            return value;
        }

        /**
         * Set value at path
         */
        set(path, value) {
            const keys = path.split('.');
            const lastKey = keys.pop();
            let target = this._data;

            for (const key of keys) {
                if (!(key in target) || typeof target[key] !== 'object') {
                    target[key] = {};
                }
                target = target[key];
            }

            const oldValue = target[lastKey];
            target[lastKey] = value;

            if (oldValue !== value) {
                this._notify(path, value, oldValue);
            }

            return this;
        }

        /**
         * Watch for changes at path
         */
        watch(path, callback, options = {}) {
            const watchId = Symbol('watch');

            if (!this._watchers.has(path)) {
                this._watchers.set(path, new Map());
            }

            this._watchers.get(path).set(watchId, { callback, options });

            // Call immediately if requested
            if (options.immediate) {
                callback(this.get(path), undefined);
            }

            // Return unwatch function
            return () => {
                const pathWatchers = this._watchers.get(path);
                if (pathWatchers) {
                    pathWatchers.delete(watchId);
                }
            };
        }

        /**
         * Define computed property
         */
        computed(path, getter, dependencies = []) {
            this._computed.set(path, {
                getter,
                dependencies,
                cache: null,
                dirty: true
            });

            // Watch dependencies
            dependencies.forEach(dep => {
                this.watch(dep, () => {
                    const computed = this._computed.get(path);
                    if (computed) {
                        computed.dirty = true;
                        this._notify(path, getter(), null);
                    }
                });
            });

            return this;
        }

        /**
         * Batch multiple updates
         */
        batch(fn) {
            this._batchUpdates = true;
            fn();
            this._batchUpdates = false;
            this._flushUpdates();
        }

        /**
         * Notify watchers of change
         */
        _notify(path, newValue, oldValue) {
            if (this._batchUpdates) {
                this._pendingUpdates.add(path);
                return;
            }

            // Notify exact path watchers
            const watchers = this._watchers.get(path);
            if (watchers) {
                watchers.forEach(({ callback, options }) => {
                    callback(newValue, oldValue);
                });
            }

            // Notify parent path watchers
            const parts = path.split('.');
            for (let i = parts.length - 1; i > 0; i--) {
                const parentPath = parts.slice(0, i).join('.');
                const parentWatchers = this._watchers.get(parentPath);
                if (parentWatchers) {
                    parentWatchers.forEach(({ callback, options }) => {
                        if (options.deep) {
                            callback(this.get(parentPath), null);
                        }
                    });
                }
            }

            // Notify wildcard watchers
            const wildcardWatchers = this._watchers.get('*');
            if (wildcardWatchers) {
                wildcardWatchers.forEach(({ callback }) => {
                    callback({ path, value: newValue, oldValue });
                });
            }
        }

        _flushUpdates() {
            this._pendingUpdates.forEach(path => {
                this._notify(path, this.get(path), null);
            });
            this._pendingUpdates.clear();
        }

        /**
         * Get all data as plain object
         */
        toJSON() {
            return JSON.parse(JSON.stringify(this._data));
        }
    }

    // ===== DOM Binder =====
    const DOMBinder = {
        _bindings: new Map(),
        _model: null,

        /**
         * Initialize with model
         */
        init: function(model) {
            this._model = model;
            return this;
        },

        /**
         * Bind all elements in container
         */
        bindAll: function(container = document) {
            // data-bind - one-way text binding
            container.querySelectorAll('[data-bind]').forEach(el => {
                this._bindText(el, el.dataset.bind);
            });

            // data-bind-html - HTML binding
            container.querySelectorAll('[data-bind-html]').forEach(el => {
                this._bindHtml(el, el.dataset.bindHtml);
            });

            // data-bind-attr - attribute binding
            container.querySelectorAll('[data-bind-attr]').forEach(el => {
                this._bindAttr(el, el.dataset.bindAttr);
            });

            // data-bind-class - class binding
            container.querySelectorAll('[data-bind-class]').forEach(el => {
                this._bindClass(el, el.dataset.bindClass);
            });

            // data-bind-style - style binding
            container.querySelectorAll('[data-bind-style]').forEach(el => {
                this._bindStyle(el, el.dataset.bindStyle);
            });

            // data-bind-visible - visibility binding
            container.querySelectorAll('[data-bind-visible]').forEach(el => {
                this._bindVisible(el, el.dataset.bindVisible);
            });

            // data-model - two-way binding for inputs
            container.querySelectorAll('[data-model]').forEach(el => {
                this._bindModel(el, el.dataset.model);
            });

            // data-bind-for - list rendering
            container.querySelectorAll('[data-bind-for]').forEach(el => {
                this._bindFor(el, el.dataset.bindFor);
            });

            // data-action - event binding
            container.querySelectorAll('[data-action]').forEach(el => {
                this._bindAction(el, el.dataset.action);
            });

            return this;
        },

        /**
         * Text content binding
         */
        _bindText: function(el, path) {
            const update = () => {
                const value = this._model.get(path);
                el.textContent = value !== undefined ? value : '';
            };

            update();
            const unwatch = this._model.watch(path, update);
            this._storeBinding(el, unwatch);
        },

        /**
         * HTML content binding
         */
        _bindHtml: function(el, path) {
            const update = () => {
                const value = this._model.get(path);
                el.innerHTML = value !== undefined ? value : '';
            };

            update();
            const unwatch = this._model.watch(path, update);
            this._storeBinding(el, unwatch);
        },

        /**
         * Attribute binding (format: "attr1:path1;attr2:path2")
         */
        _bindAttr: function(el, bindings) {
            bindings.split(';').forEach(binding => {
                const [attr, path] = binding.split(':').map(s => s.trim());
                if (!attr || !path) return;

                const update = () => {
                    const value = this._model.get(path);
                    if (value === null || value === undefined || value === false) {
                        el.removeAttribute(attr);
                    } else {
                        el.setAttribute(attr, value);
                    }
                };

                update();
                const unwatch = this._model.watch(path, update);
                this._storeBinding(el, unwatch);
            });
        },

        /**
         * Class binding (format: "class1:path1;class2:path2")
         */
        _bindClass: function(el, bindings) {
            bindings.split(';').forEach(binding => {
                const [className, path] = binding.split(':').map(s => s.trim());
                if (!className || !path) return;

                const update = () => {
                    const value = this._model.get(path);
                    el.classList.toggle(className, !!value);
                };

                update();
                const unwatch = this._model.watch(path, update);
                this._storeBinding(el, unwatch);
            });
        },

        /**
         * Style binding (format: "prop1:path1;prop2:path2")
         */
        _bindStyle: function(el, bindings) {
            bindings.split(';').forEach(binding => {
                const [prop, path] = binding.split(':').map(s => s.trim());
                if (!prop || !path) return;

                const update = () => {
                    const value = this._model.get(path);
                    el.style[prop] = value !== undefined ? value : '';
                };

                update();
                const unwatch = this._model.watch(path, update);
                this._storeBinding(el, unwatch);
            });
        },

        /**
         * Visibility binding
         */
        _bindVisible: function(el, path) {
            const update = () => {
                const value = this._model.get(path);
                el.style.display = value ? '' : 'none';
            };

            update();
            const unwatch = this._model.watch(path, update);
            this._storeBinding(el, unwatch);
        },

        /**
         * Two-way model binding for inputs
         */
        _bindModel: function(el, path) {
            const isCheckbox = el.type === 'checkbox';
            const isRadio = el.type === 'radio';
            const isSelect = el.tagName === 'SELECT';

            // Update element from model
            const updateElement = () => {
                const value = this._model.get(path);
                if (isCheckbox) {
                    el.checked = !!value;
                } else if (isRadio) {
                    el.checked = el.value === value;
                } else {
                    el.value = value !== undefined ? value : '';
                }
            };

            // Update model from element
            const updateModel = () => {
                let value;
                if (isCheckbox) {
                    value = el.checked;
                } else if (el.type === 'number' || el.type === 'range') {
                    value = parseFloat(el.value);
                } else {
                    value = el.value;
                }
                this._model.set(path, value);
            };

            updateElement();
            const unwatch = this._model.watch(path, updateElement);

            const eventType = isCheckbox || isRadio || isSelect ? 'change' : 'input';
            el.addEventListener(eventType, updateModel);

            this._storeBinding(el, () => {
                unwatch();
                el.removeEventListener(eventType, updateModel);
            });
        },

        /**
         * List rendering binding (format: "item in items")
         */
        _bindFor: function(el, expression) {
            const match = expression.match(/(\w+)\s+in\s+(\S+)/);
            if (!match) return;

            const [, itemName, path] = match;
            const template = el.innerHTML;
            const parent = el.parentNode;

            // Create placeholder comment
            const placeholder = document.createComment(`for: ${expression}`);
            parent.insertBefore(placeholder, el);
            el.remove();

            const rendered = [];

            const update = () => {
                // Remove old elements
                rendered.forEach(el => el.remove());
                rendered.length = 0;

                const items = this._model.get(path) || [];
                items.forEach((item, index) => {
                    const html = template.replace(
                        new RegExp(`\\{\\{\\s*${itemName}(\\.[^}]+)?\\s*\\}\\}`, 'g'),
                        (match, prop) => {
                            if (prop) {
                                return this._getNestedValue(item, prop.slice(1));
                            }
                            return item;
                        }
                    ).replace(/\{\{\s*\$index\s*\}\}/g, index);

                    const wrapper = document.createElement('div');
                    wrapper.innerHTML = html;

                    Array.from(wrapper.children).forEach(child => {
                        parent.insertBefore(child, placeholder.nextSibling);
                        rendered.unshift(child);
                    });
                });
            };

            update();
            const unwatch = this._model.watch(path, update, { deep: true });
            this._storeBinding(placeholder, unwatch);
        },

        /**
         * Action/event binding (format: "event:functionName" or just "functionName")
         */
        _bindAction: function(el, action) {
            const [eventName, functionName] = action.includes(':')
                ? action.split(':')
                : ['click', action];

            el.addEventListener(eventName.trim(), (e) => {
                const args = el.dataset.actionArgs ? JSON.parse(el.dataset.actionArgs) : {};
                Engine.call(functionName.trim(), { ...args, event: e.type });
            });
        },

        _getNestedValue: function(obj, path) {
            return path.split('.').reduce((o, k) => o && o[k], obj);
        },

        _storeBinding: function(el, cleanup) {
            if (!this._bindings.has(el)) {
                this._bindings.set(el, []);
            }
            this._bindings.get(el).push(cleanup);
        },

        /**
         * Unbind element
         */
        unbind: function(el) {
            const cleanups = this._bindings.get(el);
            if (cleanups) {
                cleanups.forEach(fn => fn());
                this._bindings.delete(el);
            }
        },

        /**
         * Unbind all in container
         */
        unbindAll: function(container = document) {
            this._bindings.forEach((cleanups, el) => {
                if (container.contains(el)) {
                    cleanups.forEach(fn => fn());
                    this._bindings.delete(el);
                }
            });
        }
    };

    // ===== Property Animator =====
    const PropertyAnimator = {
        _animations: new Map(),

        /**
         * Animate model property
         */
        animate: function(model, path, targetValue, options = {}) {
            const {
                duration = 300,
                easing = 'easeOutQuad',
                onComplete = null
            } = options;

            const startValue = model.get(path) || 0;
            const startTime = performance.now();
            const animId = Symbol('anim');

            // Cancel existing animation on this path
            this.cancel(path);

            const tick = (currentTime) => {
                const elapsed = currentTime - startTime;
                const progress = Math.min(elapsed / duration, 1);
                const easedProgress = this._ease(easing, progress);

                const currentValue = startValue + (targetValue - startValue) * easedProgress;
                model.set(path, currentValue);

                if (progress < 1) {
                    this._animations.set(path, requestAnimationFrame(tick));
                } else {
                    this._animations.delete(path);
                    if (onComplete) onComplete();
                }
            };

            this._animations.set(path, requestAnimationFrame(tick));
            return animId;
        },

        /**
         * Cancel animation
         */
        cancel: function(path) {
            if (this._animations.has(path)) {
                cancelAnimationFrame(this._animations.get(path));
                this._animations.delete(path);
            }
        },

        _ease: function(type, t) {
            const easings = {
                linear: t => t,
                easeInQuad: t => t * t,
                easeOutQuad: t => t * (2 - t),
                easeInOutQuad: t => t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t,
                easeInCubic: t => t * t * t,
                easeOutCubic: t => (--t) * t * t + 1,
                easeInOutCubic: t => t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1,
                easeOutElastic: t => {
                    const p = 0.3;
                    return Math.pow(2, -10 * t) * Math.sin((t - p / 4) * (2 * Math.PI) / p) + 1;
                },
                easeOutBounce: t => {
                    if (t < 1 / 2.75) return 7.5625 * t * t;
                    if (t < 2 / 2.75) return 7.5625 * (t -= 1.5 / 2.75) * t + 0.75;
                    if (t < 2.5 / 2.75) return 7.5625 * (t -= 2.25 / 2.75) * t + 0.9375;
                    return 7.5625 * (t -= 2.625 / 2.75) * t + 0.984375;
                }
            };

            return (easings[type] || easings.linear)(t);
        }
    };

    // ===== Export =====
    global.UIBinding = {
        ObservableModel,
        DOMBinder,
        PropertyAnimator,

        /**
         * Create bound model with DOM binding
         */
        createModel: function(data, container) {
            const model = new ObservableModel(data);
            DOMBinder.init(model).bindAll(container);
            return model;
        }
    };

})(window);
