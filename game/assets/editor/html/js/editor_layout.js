/**
 * Nova3D Editor - Layout Management System
 * Panel docking, resizing, and layout persistence
 * @version 2.0
 */

const EditorLayout = (function() {
    'use strict';

    // =========================================================================
    // Configuration
    // =========================================================================

    const DEFAULT_CONFIG = {
        minPanelWidth: 150,
        minPanelHeight: 100,
        resizeThreshold: 4,
        animationDuration: 200,
        storageKey: 'nova3d-editor-layout'
    };

    let config = { ...DEFAULT_CONFIG };

    // =========================================================================
    // Panel Registry
    // =========================================================================

    const panels = new Map();

    /**
     * Register a panel
     * @param {string} id - Panel identifier
     * @param {Object} options - Panel configuration
     */
    function registerPanel(id, options = {}) {
        panels.set(id, {
            id,
            element: options.element || document.getElementById(id),
            title: options.title || id,
            defaultWidth: options.defaultWidth || 250,
            defaultHeight: options.defaultHeight || 200,
            minWidth: options.minWidth || config.minPanelWidth,
            minHeight: options.minHeight || config.minPanelHeight,
            maxWidth: options.maxWidth || Infinity,
            maxHeight: options.maxHeight || Infinity,
            collapsible: options.collapsible !== false,
            closable: options.closable !== false,
            resizable: options.resizable !== false,
            dockable: options.dockable !== false,
            position: options.position || 'left',
            collapsed: false,
            visible: true,
            ...options
        });
    }

    /**
     * Get a registered panel
     * @param {string} id
     */
    function getPanel(id) {
        return panels.get(id);
    }

    // =========================================================================
    // Resize Handle System
    // =========================================================================

    class ResizeHandle {
        constructor(element, options = {}) {
            this.element = element;
            this.direction = options.direction || 'horizontal'; // 'horizontal' or 'vertical'
            this.target = options.target;
            this.minSize = options.minSize || (this.direction === 'horizontal' ? config.minPanelWidth : config.minPanelHeight);
            this.maxSize = options.maxSize || Infinity;
            this.onResize = options.onResize;
            this.onResizeEnd = options.onResizeEnd;

            this._isDragging = false;
            this._startPos = 0;
            this._startSize = 0;

            this._init();
        }

        _init() {
            this.element.classList.add('resize-handle');
            this.element.classList.add(`resize-handle-${this.direction === 'horizontal' ? 'vertical' : 'horizontal'}`);

            this.element.addEventListener('mousedown', this._onMouseDown.bind(this));
            this.element.addEventListener('touchstart', this._onTouchStart.bind(this), { passive: false });
        }

        _onMouseDown(e) {
            e.preventDefault();
            this._startDrag(this.direction === 'horizontal' ? e.clientX : e.clientY);

            document.addEventListener('mousemove', this._boundMouseMove = this._onMouseMove.bind(this));
            document.addEventListener('mouseup', this._boundMouseUp = this._onMouseUp.bind(this));
        }

        _onTouchStart(e) {
            e.preventDefault();
            const touch = e.touches[0];
            this._startDrag(this.direction === 'horizontal' ? touch.clientX : touch.clientY);

            document.addEventListener('touchmove', this._boundTouchMove = this._onTouchMove.bind(this), { passive: false });
            document.addEventListener('touchend', this._boundTouchEnd = this._onTouchEnd.bind(this));
        }

        _startDrag(pos) {
            this._isDragging = true;
            this._startPos = pos;

            const targetEl = typeof this.target === 'string' ? document.querySelector(this.target) : this.target;
            this._startSize = this.direction === 'horizontal' ? targetEl.offsetWidth : targetEl.offsetHeight;

            this.element.classList.add('dragging');
            document.body.style.cursor = this.direction === 'horizontal' ? 'col-resize' : 'row-resize';
            document.body.style.userSelect = 'none';
        }

        _onMouseMove(e) {
            this._handleMove(this.direction === 'horizontal' ? e.clientX : e.clientY);
        }

        _onTouchMove(e) {
            e.preventDefault();
            const touch = e.touches[0];
            this._handleMove(this.direction === 'horizontal' ? touch.clientX : touch.clientY);
        }

        _handleMove(pos) {
            if (!this._isDragging) return;

            const delta = pos - this._startPos;
            let newSize = this._startSize + delta;

            // Clamp size
            newSize = Math.max(this.minSize, Math.min(this.maxSize, newSize));

            const targetEl = typeof this.target === 'string' ? document.querySelector(this.target) : this.target;

            if (this.direction === 'horizontal') {
                targetEl.style.width = `${newSize}px`;
            } else {
                targetEl.style.height = `${newSize}px`;
            }

            if (this.onResize) {
                this.onResize(newSize);
            }
        }

        _onMouseUp() {
            this._endDrag();
            document.removeEventListener('mousemove', this._boundMouseMove);
            document.removeEventListener('mouseup', this._boundMouseUp);
        }

        _onTouchEnd() {
            this._endDrag();
            document.removeEventListener('touchmove', this._boundTouchMove);
            document.removeEventListener('touchend', this._boundTouchEnd);
        }

        _endDrag() {
            this._isDragging = false;
            this.element.classList.remove('dragging');
            document.body.style.cursor = '';
            document.body.style.userSelect = '';

            if (this.onResizeEnd) {
                const targetEl = typeof this.target === 'string' ? document.querySelector(this.target) : this.target;
                const size = this.direction === 'horizontal' ? targetEl.offsetWidth : targetEl.offsetHeight;
                this.onResizeEnd(size);
            }
        }

        destroy() {
            this.element.removeEventListener('mousedown', this._onMouseDown);
            this.element.removeEventListener('touchstart', this._onTouchStart);
        }
    }

    // =========================================================================
    // Panel Collapse/Expand
    // =========================================================================

    /**
     * Toggle panel collapsed state
     * @param {string} panelId
     */
    function togglePanel(panelId) {
        const panel = panels.get(panelId);
        if (!panel || !panel.collapsible) return;

        panel.collapsed = !panel.collapsed;

        if (panel.collapsed) {
            collapsePanel(panelId);
        } else {
            expandPanel(panelId);
        }
    }

    /**
     * Collapse a panel
     * @param {string} panelId
     */
    function collapsePanel(panelId) {
        const panel = panels.get(panelId);
        if (!panel) return;

        const el = panel.element;
        if (!el) return;

        // Store current size
        panel._previousWidth = el.offsetWidth;
        panel._previousHeight = el.offsetHeight;

        el.classList.add('collapsed');
        panel.collapsed = true;

        // Animate
        if (panel.position === 'left' || panel.position === 'right') {
            el.style.width = '40px';
        } else {
            el.style.height = '40px';
        }

        EditorCore?.Events?.emit('panel:collapse', { panelId });
    }

    /**
     * Expand a panel
     * @param {string} panelId
     */
    function expandPanel(panelId) {
        const panel = panels.get(panelId);
        if (!panel) return;

        const el = panel.element;
        if (!el) return;

        el.classList.remove('collapsed');
        panel.collapsed = false;

        // Restore size
        if (panel.position === 'left' || panel.position === 'right') {
            el.style.width = `${panel._previousWidth || panel.defaultWidth}px`;
        } else {
            el.style.height = `${panel._previousHeight || panel.defaultHeight}px`;
        }

        EditorCore?.Events?.emit('panel:expand', { panelId });
    }

    // =========================================================================
    // Panel Visibility
    // =========================================================================

    /**
     * Show a panel
     * @param {string} panelId
     */
    function showPanel(panelId) {
        const panel = panels.get(panelId);
        if (!panel) return;

        panel.visible = true;
        if (panel.element) {
            panel.element.style.display = '';
            panel.element.classList.remove('hidden');
        }

        EditorCore?.Events?.emit('panel:show', { panelId });
    }

    /**
     * Hide a panel
     * @param {string} panelId
     */
    function hidePanel(panelId) {
        const panel = panels.get(panelId);
        if (!panel) return;

        panel.visible = false;
        if (panel.element) {
            panel.element.classList.add('hidden');
        }

        EditorCore?.Events?.emit('panel:hide', { panelId });
    }

    /**
     * Toggle panel visibility
     * @param {string} panelId
     */
    function togglePanelVisibility(panelId) {
        const panel = panels.get(panelId);
        if (!panel) return;

        if (panel.visible) {
            hidePanel(panelId);
        } else {
            showPanel(panelId);
        }
    }

    // =========================================================================
    // Layout Save/Restore
    // =========================================================================

    /**
     * Get current layout state
     * @returns {Object}
     */
    function getLayoutState() {
        const state = {};

        panels.forEach((panel, id) => {
            if (panel.element) {
                state[id] = {
                    width: panel.element.offsetWidth,
                    height: panel.element.offsetHeight,
                    collapsed: panel.collapsed,
                    visible: panel.visible,
                    position: panel.position
                };
            }
        });

        return state;
    }

    /**
     * Apply a layout state
     * @param {Object} state
     */
    function applyLayoutState(state) {
        Object.entries(state).forEach(([id, panelState]) => {
            const panel = panels.get(id);
            if (!panel || !panel.element) return;

            const el = panel.element;

            if (panelState.width) {
                el.style.width = `${panelState.width}px`;
            }
            if (panelState.height) {
                el.style.height = `${panelState.height}px`;
            }
            if (panelState.collapsed) {
                panel.collapsed = true;
                el.classList.add('collapsed');
            }
            if (panelState.visible === false) {
                hidePanel(id);
            }
        });
    }

    /**
     * Save layout to localStorage
     */
    function saveLayout() {
        try {
            const state = getLayoutState();
            localStorage.setItem(config.storageKey, JSON.stringify(state));
        } catch (e) {
            console.warn('Failed to save layout:', e);
        }
    }

    /**
     * Load layout from localStorage
     */
    function loadLayout() {
        try {
            const saved = localStorage.getItem(config.storageKey);
            if (saved) {
                const state = JSON.parse(saved);
                applyLayoutState(state);
            }
        } catch (e) {
            console.warn('Failed to load layout:', e);
        }
    }

    /**
     * Reset layout to defaults
     */
    function resetLayout() {
        panels.forEach((panel, id) => {
            if (panel.element) {
                panel.element.style.width = `${panel.defaultWidth}px`;
                panel.element.style.height = `${panel.defaultHeight}px`;
                panel.element.classList.remove('collapsed', 'hidden');
                panel.collapsed = false;
                panel.visible = true;
            }
        });

        try {
            localStorage.removeItem(config.storageKey);
        } catch (e) {}

        EditorCore?.Events?.emit('layout:reset');
    }

    // =========================================================================
    // Split Pane
    // =========================================================================

    class SplitPane {
        constructor(container, options = {}) {
            this.container = typeof container === 'string' ? document.querySelector(container) : container;
            this.direction = options.direction || 'horizontal';
            this.initialSizes = options.sizes || [50, 50];
            this.minSizes = options.minSizes || [100, 100];
            this.gutterSize = options.gutterSize || 4;
            this.onResize = options.onResize;

            this._panes = [];
            this._gutters = [];
            this._init();
        }

        _init() {
            this.container.classList.add('split-pane');
            if (this.direction === 'vertical') {
                this.container.classList.add('split-pane-vertical');
            }

            this._panes = Array.from(this.container.children).filter(
                el => !el.classList.contains('split-pane-resizer')
            );

            // Add resizers between panes
            for (let i = 0; i < this._panes.length - 1; i++) {
                const gutter = document.createElement('div');
                gutter.className = 'split-pane-resizer';
                this._panes[i].after(gutter);
                this._gutters.push(gutter);

                new ResizeHandle(gutter, {
                    direction: this.direction,
                    target: this._panes[i],
                    minSize: this.minSizes[i],
                    onResize: () => this.onResize?.(),
                    onResizeEnd: () => this.onResize?.()
                });
            }

            // Apply initial sizes
            this._applySizes(this.initialSizes);
        }

        _applySizes(sizes) {
            this._panes.forEach((pane, i) => {
                if (sizes[i] !== undefined) {
                    if (this.direction === 'horizontal') {
                        pane.style.width = typeof sizes[i] === 'number' && sizes[i] <= 100 ?
                            `${sizes[i]}%` : `${sizes[i]}px`;
                    } else {
                        pane.style.height = typeof sizes[i] === 'number' && sizes[i] <= 100 ?
                            `${sizes[i]}%` : `${sizes[i]}px`;
                    }
                }
            });
        }

        getSizes() {
            return this._panes.map(pane =>
                this.direction === 'horizontal' ? pane.offsetWidth : pane.offsetHeight
            );
        }

        setSizes(sizes) {
            this._applySizes(sizes);
        }

        destroy() {
            this._gutters.forEach(g => g.remove());
            this.container.classList.remove('split-pane', 'split-pane-vertical');
        }
    }

    // =========================================================================
    // Auto-initialization
    // =========================================================================

    /**
     * Initialize resize handles for all panels with data-resize attribute
     */
    function initResizeHandles() {
        document.querySelectorAll('[data-resize]').forEach(handle => {
            const targetSelector = handle.dataset.resize;
            const direction = handle.dataset.resizeDirection || 'horizontal';

            new ResizeHandle(handle, {
                direction,
                target: targetSelector,
                onResizeEnd: saveLayout
            });
        });
    }

    /**
     * Initialize panels from DOM
     */
    function initPanels() {
        document.querySelectorAll('[data-panel]').forEach(el => {
            const id = el.dataset.panel;
            registerPanel(id, {
                element: el,
                title: el.dataset.panelTitle || id,
                position: el.dataset.panelPosition || 'left',
                collapsible: el.dataset.panelCollapsible !== 'false',
                closable: el.dataset.panelClosable !== 'false'
            });
        });
    }

    /**
     * Initialize the layout system
     */
    function init() {
        initPanels();
        initResizeHandles();
        loadLayout();

        // Save layout periodically and on unload
        window.addEventListener('beforeunload', saveLayout);
        setInterval(saveLayout, 30000);
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
        // Panel management
        registerPanel,
        getPanel,
        togglePanel,
        collapsePanel,
        expandPanel,
        showPanel,
        hidePanel,
        togglePanelVisibility,

        // Layout persistence
        getLayoutState,
        applyLayoutState,
        saveLayout,
        loadLayout,
        resetLayout,

        // Classes
        ResizeHandle,
        SplitPane,

        // Initialization
        init,
        initResizeHandles,
        initPanels,

        // Config
        setConfig: (newConfig) => { config = { ...config, ...newConfig }; }
    };
})();

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorLayout;
}

if (typeof window !== 'undefined') {
    window.EditorLayout = EditorLayout;
}
