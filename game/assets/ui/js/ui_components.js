/**
 * UI Components - Reusable UI component library
 */

(function(global) {
    'use strict';

    const { DOM, Events, Utils } = global.UICore || {};
    const $ = (s, c) => (c || document).querySelector(s);
    const $$ = (s, c) => Array.from((c || document).querySelectorAll(s));

    // ===== Base Component =====
    class Component {
        constructor(element, options = {}) {
            this.el = typeof element === 'string' ? $(element) : element;
            this.options = { ...this.constructor.defaults, ...options };
            this._eventHandlers = [];

            if (this.el) {
                this.el._component = this;
                this.init();
            }
        }

        static defaults = {};

        init() {}

        on(event, handler) {
            this.el.addEventListener(event, handler);
            this._eventHandlers.push({ event, handler });
            return this;
        }

        off(event, handler) {
            this.el.removeEventListener(event, handler);
            return this;
        }

        emit(event, detail) {
            this.el.dispatchEvent(new CustomEvent(event, { detail, bubbles: true }));
            return this;
        }

        destroy() {
            this._eventHandlers.forEach(({ event, handler }) => {
                this.el.removeEventListener(event, handler);
            });
            delete this.el._component;
        }

        static getInstance(el) {
            return (typeof el === 'string' ? $(el) : el)?._component;
        }
    }

    // ===== Modal Component =====
    class Modal extends Component {
        static defaults = {
            closeOnBackdrop: true,
            closeOnEscape: true,
            animation: 'fade',
            onOpen: null,
            onClose: null
        };

        init() {
            this.backdrop = this.el.querySelector('.modal-backdrop') || this._createBackdrop();
            this.content = this.el.querySelector('.modal-content');
            this.closeBtn = this.el.querySelector('[data-modal-close]');

            this._bindEvents();
        }

        _createBackdrop() {
            const backdrop = document.createElement('div');
            backdrop.className = 'modal-backdrop';
            this.el.insertBefore(backdrop, this.el.firstChild);
            return backdrop;
        }

        _bindEvents() {
            if (this.options.closeOnBackdrop) {
                this.on('click', (e) => {
                    if (e.target === this.backdrop || e.target === this.el) {
                        this.close();
                    }
                });
            }

            if (this.closeBtn) {
                this.closeBtn.addEventListener('click', () => this.close());
            }

            if (this.options.closeOnEscape) {
                this._escHandler = (e) => {
                    if (e.key === 'Escape' && this.isOpen()) {
                        this.close();
                    }
                };
                document.addEventListener('keydown', this._escHandler);
            }
        }

        open(data) {
            this.el.style.display = 'flex';
            this.el.classList.add('modal-open');

            if (this.options.animation) {
                this.content.classList.add(`animate-${this.options.animation}-in`);
            }

            if (this.options.onOpen) {
                this.options.onOpen(this, data);
            }

            this.emit('modal:open', data);
            return this;
        }

        close() {
            if (this.options.animation) {
                this.content.classList.remove(`animate-${this.options.animation}-in`);
                this.content.classList.add(`animate-${this.options.animation}-out`);

                setTimeout(() => {
                    this._hide();
                }, 300);
            } else {
                this._hide();
            }

            return this;
        }

        _hide() {
            this.el.style.display = 'none';
            this.el.classList.remove('modal-open');
            this.content.classList.remove(`animate-${this.options.animation}-out`);

            if (this.options.onClose) {
                this.options.onClose(this);
            }

            this.emit('modal:close');
        }

        isOpen() {
            return this.el.classList.contains('modal-open');
        }

        destroy() {
            if (this._escHandler) {
                document.removeEventListener('keydown', this._escHandler);
            }
            super.destroy();
        }
    }

    // ===== Tabs Component =====
    class Tabs extends Component {
        static defaults = {
            activeClass: 'active',
            animation: true,
            onChange: null
        };

        init() {
            this.tabs = $$('[data-tab]', this.el);
            this.panels = $$('[data-tab-panel]', this.el);
            this.activeTab = null;

            this._bindEvents();
            this._activateFirst();
        }

        _bindEvents() {
            this.tabs.forEach(tab => {
                tab.addEventListener('click', () => {
                    this.activate(tab.dataset.tab);
                });
            });
        }

        _activateFirst() {
            const activeTab = this.tabs.find(t => t.classList.contains(this.options.activeClass));
            if (activeTab) {
                this.activate(activeTab.dataset.tab, false);
            } else if (this.tabs.length > 0) {
                this.activate(this.tabs[0].dataset.tab, false);
            }
        }

        activate(tabId, animate = true) {
            if (this.activeTab === tabId) return;

            // Update tabs
            this.tabs.forEach(tab => {
                tab.classList.toggle(this.options.activeClass, tab.dataset.tab === tabId);
            });

            // Update panels
            this.panels.forEach(panel => {
                const isActive = panel.dataset.tabPanel === tabId;

                if (this.options.animation && animate) {
                    if (isActive) {
                        panel.style.display = '';
                        panel.classList.add('animate-fade-in');
                    } else {
                        panel.style.display = 'none';
                        panel.classList.remove('animate-fade-in');
                    }
                } else {
                    panel.style.display = isActive ? '' : 'none';
                }
            });

            const prevTab = this.activeTab;
            this.activeTab = tabId;

            if (this.options.onChange) {
                this.options.onChange(tabId, prevTab);
            }

            this.emit('tabs:change', { tab: tabId, prevTab });
            return this;
        }

        getActiveTab() {
            return this.activeTab;
        }
    }

    // ===== Dropdown Component =====
    class Dropdown extends Component {
        static defaults = {
            trigger: 'click',
            placement: 'bottom',
            offset: 4,
            closeOnSelect: true,
            onSelect: null
        };

        init() {
            this.trigger = this.el.querySelector('[data-dropdown-trigger]') || this.el;
            this.menu = this.el.querySelector('[data-dropdown-menu]');
            this.isOpen = false;

            this._bindEvents();
        }

        _bindEvents() {
            if (this.options.trigger === 'click') {
                this.trigger.addEventListener('click', (e) => {
                    e.stopPropagation();
                    this.toggle();
                });
            } else if (this.options.trigger === 'hover') {
                this.el.addEventListener('mouseenter', () => this.open());
                this.el.addEventListener('mouseleave', () => this.close());
            }

            // Close on outside click
            this._outsideClickHandler = (e) => {
                if (this.isOpen && !this.el.contains(e.target)) {
                    this.close();
                }
            };
            document.addEventListener('click', this._outsideClickHandler);

            // Handle item selection
            this.menu.addEventListener('click', (e) => {
                const item = e.target.closest('[data-dropdown-item]');
                if (item) {
                    this._onSelect(item);
                }
            });
        }

        _onSelect(item) {
            const value = item.dataset.dropdownItem || item.dataset.value;

            if (this.options.onSelect) {
                this.options.onSelect(value, item);
            }

            this.emit('dropdown:select', { value, item });

            if (this.options.closeOnSelect) {
                this.close();
            }
        }

        _position() {
            const triggerRect = this.trigger.getBoundingClientRect();
            const menuRect = this.menu.getBoundingClientRect();
            const offset = this.options.offset;

            let top, left;

            switch (this.options.placement) {
                case 'bottom':
                    top = triggerRect.bottom + offset;
                    left = triggerRect.left;
                    break;
                case 'top':
                    top = triggerRect.top - menuRect.height - offset;
                    left = triggerRect.left;
                    break;
                case 'left':
                    top = triggerRect.top;
                    left = triggerRect.left - menuRect.width - offset;
                    break;
                case 'right':
                    top = triggerRect.top;
                    left = triggerRect.right + offset;
                    break;
            }

            // Keep in viewport
            if (left + menuRect.width > window.innerWidth) {
                left = window.innerWidth - menuRect.width - offset;
            }
            if (top + menuRect.height > window.innerHeight) {
                top = triggerRect.top - menuRect.height - offset;
            }

            this.menu.style.position = 'fixed';
            this.menu.style.top = `${Math.max(0, top)}px`;
            this.menu.style.left = `${Math.max(0, left)}px`;
        }

        open() {
            if (this.isOpen) return;

            this.menu.style.display = 'block';
            this._position();
            this.el.classList.add('dropdown-open');
            this.isOpen = true;

            this.emit('dropdown:open');
            return this;
        }

        close() {
            if (!this.isOpen) return;

            this.menu.style.display = 'none';
            this.el.classList.remove('dropdown-open');
            this.isOpen = false;

            this.emit('dropdown:close');
            return this;
        }

        toggle() {
            return this.isOpen ? this.close() : this.open();
        }

        destroy() {
            document.removeEventListener('click', this._outsideClickHandler);
            super.destroy();
        }
    }

    // ===== Slider Component =====
    class Slider extends Component {
        static defaults = {
            min: 0,
            max: 100,
            step: 1,
            value: 50,
            showValue: true,
            format: (v) => v,
            onChange: null
        };

        init() {
            this.input = this.el.querySelector('input[type="range"]');
            this.valueDisplay = this.el.querySelector('[data-slider-value]');
            this.fill = this.el.querySelector('[data-slider-fill]');

            if (this.input) {
                this.input.min = this.options.min;
                this.input.max = this.options.max;
                this.input.step = this.options.step;
                this.input.value = this.options.value;
            }

            this._bindEvents();
            this._updateDisplay();
        }

        _bindEvents() {
            if (this.input) {
                this.input.addEventListener('input', () => {
                    this._updateDisplay();
                    this.emit('slider:input', { value: this.getValue() });
                });

                this.input.addEventListener('change', () => {
                    if (this.options.onChange) {
                        this.options.onChange(this.getValue());
                    }
                    this.emit('slider:change', { value: this.getValue() });
                });
            }
        }

        _updateDisplay() {
            const value = this.getValue();
            const percent = ((value - this.options.min) / (this.options.max - this.options.min)) * 100;

            if (this.valueDisplay && this.options.showValue) {
                this.valueDisplay.textContent = this.options.format(value);
            }

            if (this.fill) {
                this.fill.style.width = `${percent}%`;
            }
        }

        getValue() {
            return this.input ? parseFloat(this.input.value) : this.options.value;
        }

        setValue(value) {
            if (this.input) {
                this.input.value = value;
                this._updateDisplay();
            }
            return this;
        }
    }

    // ===== Progress Bar Component =====
    class ProgressBar extends Component {
        static defaults = {
            value: 0,
            max: 100,
            showText: true,
            format: (v, max) => `${Math.round(v / max * 100)}%`,
            animated: true
        };

        init() {
            this.fill = this.el.querySelector('[data-progress-fill]') || this._createFill();
            this.text = this.el.querySelector('[data-progress-text]');

            this.setValue(this.options.value);
        }

        _createFill() {
            const fill = document.createElement('div');
            fill.className = 'progress-fill';
            fill.setAttribute('data-progress-fill', '');
            this.el.appendChild(fill);
            return fill;
        }

        setValue(value, animate = true) {
            const percent = Math.min(100, Math.max(0, (value / this.options.max) * 100));

            if (this.options.animated && animate) {
                this.fill.style.transition = 'width 0.3s ease';
            } else {
                this.fill.style.transition = 'none';
            }

            this.fill.style.width = `${percent}%`;

            if (this.text && this.options.showText) {
                this.text.textContent = this.options.format(value, this.options.max);
            }

            this.emit('progress:change', { value, percent });
            return this;
        }

        getValue() {
            return parseFloat(this.fill.style.width) / 100 * this.options.max;
        }
    }

    // ===== Tooltip Component =====
    class Tooltip extends Component {
        static defaults = {
            placement: 'top',
            offset: 8,
            delay: 200,
            trigger: 'hover'
        };

        init() {
            this.content = this.el.dataset.tooltip || this.el.title;
            this.el.title = '';
            this.tooltipEl = null;
            this.timeout = null;

            this._bindEvents();
        }

        _bindEvents() {
            if (this.options.trigger === 'hover') {
                this.el.addEventListener('mouseenter', () => this._scheduleShow());
                this.el.addEventListener('mouseleave', () => this._scheduleHide());
            } else if (this.options.trigger === 'click') {
                this.el.addEventListener('click', () => this.toggle());
            }
        }

        _scheduleShow() {
            clearTimeout(this.timeout);
            this.timeout = setTimeout(() => this.show(), this.options.delay);
        }

        _scheduleHide() {
            clearTimeout(this.timeout);
            this.timeout = setTimeout(() => this.hide(), 100);
        }

        _createTooltip() {
            const tooltip = document.createElement('div');
            tooltip.className = 'tooltip';
            tooltip.innerHTML = this.content;
            document.body.appendChild(tooltip);
            return tooltip;
        }

        _position() {
            if (!this.tooltipEl) return;

            const targetRect = this.el.getBoundingClientRect();
            const tooltipRect = this.tooltipEl.getBoundingClientRect();
            const offset = this.options.offset;

            let top, left;

            switch (this.options.placement) {
                case 'top':
                    top = targetRect.top - tooltipRect.height - offset;
                    left = targetRect.left + (targetRect.width - tooltipRect.width) / 2;
                    break;
                case 'bottom':
                    top = targetRect.bottom + offset;
                    left = targetRect.left + (targetRect.width - tooltipRect.width) / 2;
                    break;
                case 'left':
                    top = targetRect.top + (targetRect.height - tooltipRect.height) / 2;
                    left = targetRect.left - tooltipRect.width - offset;
                    break;
                case 'right':
                    top = targetRect.top + (targetRect.height - tooltipRect.height) / 2;
                    left = targetRect.right + offset;
                    break;
            }

            // Keep in viewport
            left = Math.max(offset, Math.min(left, window.innerWidth - tooltipRect.width - offset));
            top = Math.max(offset, Math.min(top, window.innerHeight - tooltipRect.height - offset));

            this.tooltipEl.style.top = `${top}px`;
            this.tooltipEl.style.left = `${left}px`;
        }

        show() {
            if (this.tooltipEl) return;

            this.tooltipEl = this._createTooltip();
            this._position();
            this.tooltipEl.classList.add('animate-fade-in');

            return this;
        }

        hide() {
            if (!this.tooltipEl) return;

            this.tooltipEl.remove();
            this.tooltipEl = null;

            return this;
        }

        toggle() {
            return this.tooltipEl ? this.hide() : this.show();
        }

        setContent(content) {
            this.content = content;
            if (this.tooltipEl) {
                this.tooltipEl.innerHTML = content;
                this._position();
            }
            return this;
        }

        destroy() {
            this.hide();
            clearTimeout(this.timeout);
            super.destroy();
        }
    }

    // ===== Context Menu Component =====
    class ContextMenu extends Component {
        static defaults = {
            items: [],
            onSelect: null
        };

        init() {
            this.menu = null;
            this._bindEvents();
        }

        _bindEvents() {
            this.el.addEventListener('contextmenu', (e) => {
                e.preventDefault();
                this.show(e.clientX, e.clientY);
            });

            this._outsideClickHandler = () => this.hide();
            document.addEventListener('click', this._outsideClickHandler);
        }

        _createMenu(x, y) {
            const menu = document.createElement('div');
            menu.className = 'context-menu';
            menu.style.position = 'fixed';
            menu.style.left = `${x}px`;
            menu.style.top = `${y}px`;

            this.options.items.forEach((item, index) => {
                if (item.separator) {
                    const sep = document.createElement('div');
                    sep.className = 'context-menu-separator';
                    menu.appendChild(sep);
                } else {
                    const menuItem = document.createElement('div');
                    menuItem.className = 'context-menu-item';
                    if (item.disabled) menuItem.classList.add('disabled');
                    menuItem.innerHTML = `
                        ${item.icon ? `<span class="menu-icon">${item.icon}</span>` : ''}
                        <span class="menu-label">${item.label}</span>
                        ${item.shortcut ? `<span class="menu-shortcut">${item.shortcut}</span>` : ''}
                    `;

                    if (!item.disabled) {
                        menuItem.addEventListener('click', () => {
                            if (this.options.onSelect) {
                                this.options.onSelect(item.action || item.label, item);
                            }
                            this.emit('contextmenu:select', { action: item.action, item });
                            this.hide();
                        });
                    }

                    menu.appendChild(menuItem);
                }
            });

            document.body.appendChild(menu);

            // Adjust position if off-screen
            const rect = menu.getBoundingClientRect();
            if (rect.right > window.innerWidth) {
                menu.style.left = `${x - rect.width}px`;
            }
            if (rect.bottom > window.innerHeight) {
                menu.style.top = `${y - rect.height}px`;
            }

            return menu;
        }

        show(x, y) {
            this.hide();
            this.menu = this._createMenu(x, y);
            this.menu.classList.add('animate-scale-in');
            return this;
        }

        hide() {
            if (this.menu) {
                this.menu.remove();
                this.menu = null;
            }
            return this;
        }

        setItems(items) {
            this.options.items = items;
            return this;
        }

        destroy() {
            this.hide();
            document.removeEventListener('click', this._outsideClickHandler);
            super.destroy();
        }
    }

    // ===== Drag and Drop Component =====
    class Draggable extends Component {
        static defaults = {
            handle: null,
            axis: null,
            bounds: null,
            grid: null,
            onDragStart: null,
            onDrag: null,
            onDragEnd: null
        };

        init() {
            this.handle = this.options.handle ? $(this.options.handle, this.el) : this.el;
            this.isDragging = false;
            this.startPos = { x: 0, y: 0 };
            this.currentPos = { x: 0, y: 0 };

            this._bindEvents();
        }

        _bindEvents() {
            this.handle.addEventListener('mousedown', (e) => this._onStart(e));
            document.addEventListener('mousemove', (e) => this._onMove(e));
            document.addEventListener('mouseup', (e) => this._onEnd(e));

            // Touch support
            this.handle.addEventListener('touchstart', (e) => this._onStart(e.touches[0]));
            document.addEventListener('touchmove', (e) => this._onMove(e.touches[0]));
            document.addEventListener('touchend', (e) => this._onEnd(e));
        }

        _onStart(e) {
            this.isDragging = true;
            this.startPos = { x: e.clientX, y: e.clientY };

            const rect = this.el.getBoundingClientRect();
            this.offset = {
                x: e.clientX - rect.left,
                y: e.clientY - rect.top
            };

            this.el.classList.add('dragging');

            if (this.options.onDragStart) {
                this.options.onDragStart(this.currentPos);
            }

            this.emit('drag:start', this.currentPos);
        }

        _onMove(e) {
            if (!this.isDragging) return;

            let x = e.clientX - this.offset.x;
            let y = e.clientY - this.offset.y;

            // Apply axis constraint
            if (this.options.axis === 'x') {
                y = this.el.offsetTop;
            } else if (this.options.axis === 'y') {
                x = this.el.offsetLeft;
            }

            // Apply grid snapping
            if (this.options.grid) {
                x = Math.round(x / this.options.grid[0]) * this.options.grid[0];
                y = Math.round(y / this.options.grid[1]) * this.options.grid[1];
            }

            // Apply bounds
            if (this.options.bounds) {
                const bounds = typeof this.options.bounds === 'string'
                    ? $(this.options.bounds).getBoundingClientRect()
                    : this.options.bounds;

                x = Math.max(bounds.left, Math.min(x, bounds.right - this.el.offsetWidth));
                y = Math.max(bounds.top, Math.min(y, bounds.bottom - this.el.offsetHeight));
            }

            this.currentPos = { x, y };
            this.el.style.left = `${x}px`;
            this.el.style.top = `${y}px`;

            if (this.options.onDrag) {
                this.options.onDrag(this.currentPos);
            }

            this.emit('drag:move', this.currentPos);
        }

        _onEnd() {
            if (!this.isDragging) return;

            this.isDragging = false;
            this.el.classList.remove('dragging');

            if (this.options.onDragEnd) {
                this.options.onDragEnd(this.currentPos);
            }

            this.emit('drag:end', this.currentPos);
        }
    }

    // ===== Auto-initialize components =====
    function autoInit(container = document) {
        container.querySelectorAll('[data-component]').forEach(el => {
            const componentName = el.dataset.component;
            const options = el.dataset.options ? JSON.parse(el.dataset.options) : {};

            const ComponentClass = UIComponents[componentName];
            if (ComponentClass) {
                new ComponentClass(el, options);
            }
        });
    }

    // ===== Export =====
    global.UIComponents = {
        Component,
        Modal,
        Tabs,
        Dropdown,
        Slider,
        ProgressBar,
        Tooltip,
        ContextMenu,
        Draggable,
        autoInit
    };

    // Auto-initialize on DOM ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => autoInit());
    } else {
        autoInit();
    }

})(window);
