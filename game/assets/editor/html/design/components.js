/**
 * Nova3D Editor Design System - Web Components
 * Reusable UI components as custom elements
 * @version 2.0
 */

// ============================================================================
// Base Component Class
// ============================================================================

class NovaComponent extends HTMLElement {
    constructor() {
        super();
        this._connected = false;
        this._eventListeners = [];
    }

    connectedCallback() {
        this._connected = true;
        this.render();
        this.setup();
    }

    disconnectedCallback() {
        this._connected = false;
        this._eventListeners.forEach(({ el, event, handler }) => {
            el.removeEventListener(event, handler);
        });
        this._eventListeners = [];
        this.cleanup();
    }

    attributeChangedCallback(name, oldValue, newValue) {
        if (this._connected && oldValue !== newValue) {
            this.onAttributeChange(name, oldValue, newValue);
        }
    }

    render() {}
    setup() {}
    cleanup() {}
    onAttributeChange(name, oldValue, newValue) {}

    on(el, event, handler) {
        el.addEventListener(event, handler);
        this._eventListeners.push({ el, event, handler });
    }

    emit(eventName, detail = {}) {
        this.dispatchEvent(new CustomEvent(eventName, {
            detail,
            bubbles: true,
            composed: true
        }));
    }

    $(selector) {
        return this.querySelector(selector);
    }

    $$(selector) {
        return this.querySelectorAll(selector);
    }
}

// ============================================================================
// Panel Component
// ============================================================================

class NovaPanel extends NovaComponent {
    static get observedAttributes() {
        return ['title', 'collapsible', 'collapsed'];
    }

    render() {
        const title = this.getAttribute('title') || '';
        const collapsible = this.hasAttribute('collapsible');
        const collapsed = this.hasAttribute('collapsed');

        this.classList.add('panel');
        if (collapsed) this.classList.add('collapsed');

        this.innerHTML = `
            <div class="panel-header" ${collapsible ? 'role="button" tabindex="0"' : ''}>
                ${collapsible ? '<span class="panel-toggle">&#x25BC;</span>' : ''}
                <span class="panel-title">${title}</span>
                <div class="panel-actions"><slot name="actions"></slot></div>
            </div>
            <div class="panel-content"><slot></slot></div>
        `;
    }

    setup() {
        if (this.hasAttribute('collapsible')) {
            const header = this.$('.panel-header');
            this.on(header, 'click', () => this.toggle());
            this.on(header, 'keydown', (e) => {
                if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    this.toggle();
                }
            });
        }
    }

    toggle() {
        const isCollapsed = this.classList.toggle('collapsed');
        this.emit('toggle', { collapsed: isCollapsed });
    }

    expand() {
        this.classList.remove('collapsed');
        this.emit('toggle', { collapsed: false });
    }

    collapse() {
        this.classList.add('collapsed');
        this.emit('toggle', { collapsed: true });
    }
}

// ============================================================================
// Tab Container Component
// ============================================================================

class NovaTabContainer extends NovaComponent {
    static get observedAttributes() {
        return ['active'];
    }

    render() {
        this.classList.add('tabs');

        const tabs = Array.from(this.querySelectorAll('nova-tab'));
        const panels = Array.from(this.querySelectorAll('nova-tab-panel'));

        if (!this.$('.tab-list')) {
            const tabList = document.createElement('div');
            tabList.className = 'tab-list';
            tabList.setAttribute('role', 'tablist');

            tabs.forEach((tab, index) => {
                const button = document.createElement('button');
                button.className = 'tab';
                button.setAttribute('role', 'tab');
                button.setAttribute('data-index', index);
                button.textContent = tab.getAttribute('label') || `Tab ${index + 1}`;
                tabList.appendChild(button);
            });

            this.insertBefore(tabList, this.firstChild);
        }

        const activeIndex = parseInt(this.getAttribute('active') || '0');
        this.setActiveTab(activeIndex);
    }

    setup() {
        this.on(this.$('.tab-list'), 'click', (e) => {
            const tab = e.target.closest('.tab');
            if (tab) {
                const index = parseInt(tab.getAttribute('data-index'));
                this.setActiveTab(index);
            }
        });
    }

    setActiveTab(index) {
        const tabs = this.$$('.tab');
        const panels = this.querySelectorAll('nova-tab-panel');

        tabs.forEach((tab, i) => {
            tab.classList.toggle('active', i === index);
            tab.setAttribute('aria-selected', i === index);
        });

        panels.forEach((panel, i) => {
            panel.classList.toggle('active', i === index);
            panel.setAttribute('aria-hidden', i !== index);
        });

        this.emit('tab-change', { index });
    }

    onAttributeChange(name, oldValue, newValue) {
        if (name === 'active') {
            this.setActiveTab(parseInt(newValue || '0'));
        }
    }
}

class NovaTab extends HTMLElement {
    connectedCallback() {
        this.style.display = 'none';
    }
}

class NovaTabPanel extends HTMLElement {
    connectedCallback() {
        this.classList.add('tab-panel');
        this.setAttribute('role', 'tabpanel');
    }
}

// ============================================================================
// Tree View Component
// ============================================================================

class NovaTreeView extends NovaComponent {
    static get observedAttributes() {
        return ['data'];
    }

    get items() {
        return this._items || [];
    }

    set items(data) {
        this._items = data;
        this.renderTree();
    }

    render() {
        this.classList.add('tree');
        this.setAttribute('role', 'tree');
    }

    renderTree() {
        this.innerHTML = '';
        this.renderItems(this._items || [], this, 0);
    }

    renderItems(items, container, depth) {
        items.forEach(item => {
            const li = document.createElement('div');
            li.className = 'tree-item';
            li.setAttribute('role', 'treeitem');
            li.setAttribute('data-id', item.id);
            li.style.paddingLeft = `${depth * 16 + 8}px`;

            const hasChildren = item.children && item.children.length > 0;

            li.innerHTML = `
                ${hasChildren ? '<span class="tree-item-toggle">&#x25B6;</span>' : '<span class="tree-item-toggle" style="visibility:hidden">&#x25B6;</span>'}
                ${item.icon ? `<span class="tree-item-icon">${item.icon}</span>` : ''}
                <span class="tree-item-label">${item.label}</span>
                ${item.count !== undefined ? `<span class="tree-item-count">${item.count}</span>` : ''}
            `;

            container.appendChild(li);

            if (hasChildren) {
                const childContainer = document.createElement('div');
                childContainer.className = 'tree-children';
                childContainer.style.display = 'none';
                container.appendChild(childContainer);
                this.renderItems(item.children, childContainer, depth + 1);
            }
        });
    }

    setup() {
        this.on(this, 'click', (e) => {
            const item = e.target.closest('.tree-item');
            if (!item) return;

            const toggle = e.target.closest('.tree-item-toggle');
            const id = item.getAttribute('data-id');

            if (toggle) {
                this.toggleExpand(item);
            } else {
                this.selectItem(item);
            }
        });
    }

    toggleExpand(item) {
        const children = item.nextElementSibling;
        if (children && children.classList.contains('tree-children')) {
            const isExpanded = children.style.display !== 'none';
            children.style.display = isExpanded ? 'none' : 'block';
            item.classList.toggle('expanded', !isExpanded);
            this.emit('expand', { id: item.getAttribute('data-id'), expanded: !isExpanded });
        }
    }

    selectItem(item) {
        this.$$('.tree-item.selected').forEach(el => el.classList.remove('selected'));
        item.classList.add('selected');
        this.emit('select', { id: item.getAttribute('data-id') });
    }
}

// ============================================================================
// Property Grid Component
// ============================================================================

class NovaPropertyGrid extends NovaComponent {
    get data() {
        return this._data || {};
    }

    set data(value) {
        this._data = value;
        this.renderProperties();
    }

    get schema() {
        return this._schema || [];
    }

    set schema(value) {
        this._schema = value;
        this.renderProperties();
    }

    render() {
        this.classList.add('property-grid');
    }

    renderProperties() {
        this.innerHTML = '';

        (this._schema || []).forEach(prop => {
            const group = document.createElement('div');
            group.className = 'form-group';

            const label = document.createElement('label');
            label.className = 'form-label';
            label.textContent = prop.label || prop.name;
            if (prop.required) label.classList.add('required');
            group.appendChild(label);

            const input = this.createInput(prop);
            group.appendChild(input);

            if (prop.hint) {
                const hint = document.createElement('div');
                hint.className = 'form-hint';
                hint.textContent = prop.hint;
                group.appendChild(hint);
            }

            this.appendChild(group);
        });
    }

    createInput(prop) {
        const value = this.getNestedValue(this._data, prop.name);
        let input;

        switch (prop.type) {
            case 'text':
            case 'string':
                input = document.createElement('input');
                input.type = 'text';
                input.value = value || '';
                break;

            case 'number':
                input = document.createElement('input');
                input.type = 'number';
                input.value = value !== undefined ? value : '';
                if (prop.min !== undefined) input.min = prop.min;
                if (prop.max !== undefined) input.max = prop.max;
                if (prop.step !== undefined) input.step = prop.step;
                break;

            case 'boolean':
                input = document.createElement('input');
                input.type = 'checkbox';
                input.checked = !!value;
                break;

            case 'select':
                input = document.createElement('select');
                (prop.options || []).forEach(opt => {
                    const option = document.createElement('option');
                    option.value = typeof opt === 'object' ? opt.value : opt;
                    option.textContent = typeof opt === 'object' ? opt.label : opt;
                    option.selected = option.value === value;
                    input.appendChild(option);
                });
                break;

            case 'textarea':
                input = document.createElement('textarea');
                input.value = value || '';
                input.rows = prop.rows || 3;
                break;

            case 'color':
                input = document.createElement('input');
                input.type = 'color';
                input.value = value || '#ffffff';
                break;

            case 'vector2':
                input = document.createElement('nova-vector-input');
                input.setAttribute('dimensions', '2');
                if (value) input.value = value;
                break;

            case 'vector3':
                input = document.createElement('nova-vector-input');
                input.setAttribute('dimensions', '3');
                if (value) input.value = value;
                break;

            default:
                input = document.createElement('input');
                input.type = 'text';
                input.value = value || '';
        }

        input.setAttribute('data-path', prop.name);
        if (prop.readonly) input.readOnly = true;
        if (prop.disabled) input.disabled = true;

        input.addEventListener('change', () => {
            this.handleChange(prop.name, input);
        });

        return input;
    }

    handleChange(path, input) {
        let value;
        if (input.type === 'checkbox') {
            value = input.checked;
        } else if (input.type === 'number') {
            value = parseFloat(input.value) || 0;
        } else if (input.tagName === 'NOVA-VECTOR-INPUT') {
            value = input.value;
        } else {
            value = input.value;
        }

        this.setNestedValue(this._data, path, value);
        this.emit('change', { path, value, data: this._data });
    }

    getNestedValue(obj, path) {
        return path.split('.').reduce((current, key) =>
            current && current[key] !== undefined ? current[key] : undefined, obj);
    }

    setNestedValue(obj, path, value) {
        const keys = path.split('.');
        const lastKey = keys.pop();
        const target = keys.reduce((current, key) => {
            if (!current[key]) current[key] = {};
            return current[key];
        }, obj);
        target[lastKey] = value;
    }
}

// ============================================================================
// Color Picker Component
// ============================================================================

class NovaColorPicker extends NovaComponent {
    static get observedAttributes() {
        return ['value', 'alpha'];
    }

    get value() {
        return this._value || '#ffffff';
    }

    set value(val) {
        this._value = val;
        this.updateDisplay();
    }

    render() {
        this.classList.add('color-picker');
        this.innerHTML = `
            <div class="color-picker-swatch" tabindex="0">
                <div class="color-picker-preview"></div>
            </div>
            <input type="text" class="color-picker-input" maxlength="9">
            <div class="color-picker-dropdown">
                <div class="color-picker-gradient">
                    <div class="color-picker-cursor"></div>
                </div>
                <div class="color-picker-hue">
                    <div class="color-picker-hue-cursor"></div>
                </div>
                ${this.hasAttribute('alpha') ? `
                    <div class="color-picker-alpha">
                        <div class="color-picker-alpha-cursor"></div>
                    </div>
                ` : ''}
                <div class="color-picker-presets">
                    ${this.getPresetColors().map(c =>
                        `<div class="color-picker-preset" data-color="${c}" style="background:${c}"></div>`
                    ).join('')}
                </div>
            </div>
        `;

        this.updateDisplay();
    }

    getPresetColors() {
        return [
            '#ef4444', '#f97316', '#eab308', '#22c55e', '#06b6d4',
            '#3b82f6', '#8b5cf6', '#ec4899', '#ffffff', '#000000',
            '#6b7280', '#9ca3af', '#d1d5db', '#f3f4f6', '#fef3c7'
        ];
    }

    setup() {
        const swatch = this.$('.color-picker-swatch');
        const input = this.$('.color-picker-input');
        const dropdown = this.$('.color-picker-dropdown');

        this.on(swatch, 'click', () => {
            dropdown.classList.toggle('visible');
        });

        this.on(input, 'change', () => {
            this.value = input.value;
            this.emit('change', { value: this.value });
        });

        this.on(this.$('.color-picker-presets'), 'click', (e) => {
            const preset = e.target.closest('.color-picker-preset');
            if (preset) {
                this.value = preset.getAttribute('data-color');
                this.emit('change', { value: this.value });
            }
        });

        this.on(document, 'click', (e) => {
            if (!this.contains(e.target)) {
                dropdown.classList.remove('visible');
            }
        });
    }

    updateDisplay() {
        const preview = this.$('.color-picker-preview');
        const input = this.$('.color-picker-input');
        if (preview) preview.style.background = this._value;
        if (input) input.value = this._value;
    }

    onAttributeChange(name, oldValue, newValue) {
        if (name === 'value') {
            this._value = newValue;
            this.updateDisplay();
        }
    }
}

// ============================================================================
// Number Slider Component
// ============================================================================

class NovaNumberSlider extends NovaComponent {
    static get observedAttributes() {
        return ['value', 'min', 'max', 'step', 'label'];
    }

    get value() {
        return parseFloat(this._value) || 0;
    }

    set value(val) {
        this._value = val;
        this.updateDisplay();
    }

    render() {
        const min = this.getAttribute('min') || 0;
        const max = this.getAttribute('max') || 100;
        const step = this.getAttribute('step') || 1;
        const label = this.getAttribute('label') || '';

        this.classList.add('number-slider');
        this.innerHTML = `
            ${label ? `<label class="form-label">${label}</label>` : ''}
            <div class="number-slider-track">
                <input type="range" min="${min}" max="${max}" step="${step}" value="${this._value || min}">
                <input type="number" min="${min}" max="${max}" step="${step}" value="${this._value || min}">
            </div>
        `;
    }

    setup() {
        const range = this.$('input[type="range"]');
        const number = this.$('input[type="number"]');

        this.on(range, 'input', () => {
            this._value = range.value;
            number.value = range.value;
            this.emit('input', { value: this.value });
        });

        this.on(range, 'change', () => {
            this.emit('change', { value: this.value });
        });

        this.on(number, 'change', () => {
            this._value = number.value;
            range.value = number.value;
            this.emit('change', { value: this.value });
        });
    }

    updateDisplay() {
        const range = this.$('input[type="range"]');
        const number = this.$('input[type="number"]');
        if (range) range.value = this._value;
        if (number) number.value = this._value;
    }

    onAttributeChange(name, oldValue, newValue) {
        if (name === 'value') {
            this._value = newValue;
            this.updateDisplay();
        }
    }
}

// ============================================================================
// Vector Input Component
// ============================================================================

class NovaVectorInput extends NovaComponent {
    static get observedAttributes() {
        return ['dimensions', 'labels', 'step'];
    }

    get value() {
        const inputs = this.$$('input');
        const values = Array.from(inputs).map(i => parseFloat(i.value) || 0);
        return values.length === 1 ? values[0] : values;
    }

    set value(val) {
        const inputs = this.$$('input');
        const values = Array.isArray(val) ? val : [val];
        inputs.forEach((input, i) => {
            input.value = values[i] !== undefined ? values[i] : 0;
        });
    }

    render() {
        const dims = parseInt(this.getAttribute('dimensions') || '3');
        const labels = (this.getAttribute('labels') || 'X,Y,Z,W').split(',');
        const step = this.getAttribute('step') || '0.1';

        this.classList.add('vector-input');
        this.innerHTML = Array.from({ length: dims }, (_, i) => `
            <div class="vector-input-field">
                <label>${labels[i] || ''}</label>
                <input type="number" step="${step}" value="0" data-index="${i}">
            </div>
        `).join('');
    }

    setup() {
        this.on(this, 'change', (e) => {
            if (e.target.tagName === 'INPUT') {
                this.emit('change', { value: this.value });
            }
        });
    }
}

// ============================================================================
// Search Box Component
// ============================================================================

class NovaSearchBox extends NovaComponent {
    static get observedAttributes() {
        return ['placeholder', 'value', 'debounce'];
    }

    get value() {
        const input = this.$('input');
        return input ? input.value : '';
    }

    set value(val) {
        const input = this.$('input');
        if (input) input.value = val;
        this.updateClearButton();
    }

    render() {
        const placeholder = this.getAttribute('placeholder') || 'Search...';

        this.classList.add('search-box');
        this.innerHTML = `
            <div class="input-wrapper has-icon-left has-icon-right">
                <span class="input-icon-left">&#x1F50D;</span>
                <input type="search" placeholder="${placeholder}">
                <button class="input-icon-right search-clear" style="display:none">&times;</button>
            </div>
        `;
    }

    setup() {
        const input = this.$('input');
        const clear = this.$('.search-clear');
        const debounceTime = parseInt(this.getAttribute('debounce') || '200');

        let debounceTimer;
        this.on(input, 'input', () => {
            this.updateClearButton();
            clearTimeout(debounceTimer);
            debounceTimer = setTimeout(() => {
                this.emit('search', { value: input.value });
            }, debounceTime);
        });

        this.on(clear, 'click', () => {
            input.value = '';
            this.updateClearButton();
            input.focus();
            this.emit('search', { value: '' });
            this.emit('clear');
        });

        this.on(input, 'keydown', (e) => {
            if (e.key === 'Escape') {
                input.value = '';
                this.updateClearButton();
                this.emit('search', { value: '' });
                this.emit('clear');
            }
        });
    }

    updateClearButton() {
        const input = this.$('input');
        const clear = this.$('.search-clear');
        if (clear) {
            clear.style.display = input.value ? 'flex' : 'none';
        }
    }
}

// ============================================================================
// Modal Component
// ============================================================================

class NovaModal extends NovaComponent {
    static get observedAttributes() {
        return ['title', 'size', 'open'];
    }

    render() {
        const title = this.getAttribute('title') || '';
        const size = this.getAttribute('size') || 'md';

        this.classList.add('modal-overlay');
        this.innerHTML = `
            <div class="modal modal-${size}" role="dialog" aria-modal="true">
                <div class="modal-header">
                    <h3 class="modal-title">${title}</h3>
                    <button class="modal-close" aria-label="Close">&times;</button>
                </div>
                <div class="modal-body"><slot></slot></div>
                <div class="modal-footer"><slot name="footer"></slot></div>
            </div>
        `;
    }

    setup() {
        this.on(this.$('.modal-close'), 'click', () => this.close());
        this.on(this, 'click', (e) => {
            if (e.target === this) this.close();
        });
        this.on(document, 'keydown', (e) => {
            if (e.key === 'Escape' && this.classList.contains('open')) {
                this.close();
            }
        });
    }

    open() {
        this.classList.add('open');
        document.body.style.overflow = 'hidden';
        this.emit('open');
    }

    close() {
        this.classList.remove('open');
        document.body.style.overflow = '';
        this.emit('close');
    }

    onAttributeChange(name, oldValue, newValue) {
        if (name === 'open') {
            if (this.hasAttribute('open')) {
                this.open();
            } else {
                this.close();
            }
        }
    }
}

// ============================================================================
// Toast Notification System
// ============================================================================

class NovaToast {
    static container = null;

    static getContainer() {
        if (!this.container) {
            this.container = document.createElement('div');
            this.container.className = 'toast-container';
            document.body.appendChild(this.container);
        }
        return this.container;
    }

    static show(options = {}) {
        const {
            title = '',
            message = '',
            type = 'info',
            duration = 5000,
            closable = true
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
            </div>
            ${closable ? '<button class="toast-close">&times;</button>' : ''}
        `;

        const container = this.getContainer();
        container.appendChild(toast);

        if (closable) {
            toast.querySelector('.toast-close').addEventListener('click', () => {
                this.dismiss(toast);
            });
        }

        if (duration > 0) {
            setTimeout(() => this.dismiss(toast), duration);
        }

        return toast;
    }

    static dismiss(toast) {
        toast.style.animation = 'fade-out 0.2s ease-out forwards';
        setTimeout(() => toast.remove(), 200);
    }

    static success(message, title = '') {
        return this.show({ message, title, type: 'success' });
    }

    static warning(message, title = '') {
        return this.show({ message, title, type: 'warning' });
    }

    static error(message, title = '') {
        return this.show({ message, title, type: 'error' });
    }

    static info(message, title = '') {
        return this.show({ message, title, type: 'info' });
    }
}

// ============================================================================
// Context Menu System
// ============================================================================

class NovaContextMenu {
    static menu = null;

    static show(x, y, items) {
        this.hide();

        const menu = document.createElement('div');
        menu.className = 'context-menu visible';
        menu.style.left = `${x}px`;
        menu.style.top = `${y}px`;

        items.forEach(item => {
            if (item.separator) {
                const sep = document.createElement('div');
                sep.className = 'menu-separator';
                menu.appendChild(sep);
            } else {
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
                        item.onClick();
                        this.hide();
                    });
                }

                menu.appendChild(menuItem);
            }
        });

        document.body.appendChild(menu);
        this.menu = menu;

        // Position adjustment
        const rect = menu.getBoundingClientRect();
        if (rect.right > window.innerWidth) {
            menu.style.left = `${x - rect.width}px`;
        }
        if (rect.bottom > window.innerHeight) {
            menu.style.top = `${y - rect.height}px`;
        }

        // Close on click outside
        const closeHandler = (e) => {
            if (!menu.contains(e.target)) {
                this.hide();
                document.removeEventListener('click', closeHandler);
            }
        };
        setTimeout(() => document.addEventListener('click', closeHandler), 0);

        return menu;
    }

    static hide() {
        if (this.menu) {
            this.menu.remove();
            this.menu = null;
        }
    }
}

// ============================================================================
// Register Custom Elements
// ============================================================================

customElements.define('nova-panel', NovaPanel);
customElements.define('nova-tab-container', NovaTabContainer);
customElements.define('nova-tab', NovaTab);
customElements.define('nova-tab-panel', NovaTabPanel);
customElements.define('nova-tree-view', NovaTreeView);
customElements.define('nova-property-grid', NovaPropertyGrid);
customElements.define('nova-color-picker', NovaColorPicker);
customElements.define('nova-number-slider', NovaNumberSlider);
customElements.define('nova-vector-input', NovaVectorInput);
customElements.define('nova-search-box', NovaSearchBox);
customElements.define('nova-modal', NovaModal);

// ============================================================================
// Exports
// ============================================================================

if (typeof window !== 'undefined') {
    window.NovaComponents = {
        NovaComponent,
        NovaPanel,
        NovaTabContainer,
        NovaTab,
        NovaTabPanel,
        NovaTreeView,
        NovaPropertyGrid,
        NovaColorPicker,
        NovaNumberSlider,
        NovaVectorInput,
        NovaSearchBox,
        NovaModal,
        NovaToast,
        NovaContextMenu
    };
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = window.NovaComponents;
}
