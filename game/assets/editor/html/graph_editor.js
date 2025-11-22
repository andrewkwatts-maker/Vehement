/**
 * Nova3D/Vehement2 Graph Editor JavaScript
 * Node-based visual editor for tech trees, behavior trees, etc.
 */

/**
 * GraphEditor class
 */
function GraphEditor(canvasId, options) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas ? this.canvas.getContext('2d') : null;
    this.options = options || {};

    // Containers
    this.nodesContainer = document.getElementById(this.options.nodesContainer || 'graph-nodes');
    this.connectionsContainer = document.getElementById(this.options.connectionsContainer || 'graph-connections');

    // Node types configuration
    this.nodeTypes = this.options.nodeTypes || {};

    // State
    this.nodes = [];
    this.connections = [];
    this.selectedNodes = new Set();
    this.selectedConnections = new Set();

    // Viewport
    this.viewport = {
        panX: 0,
        panY: 0,
        zoom: 1,
        minZoom: 0.1,
        maxZoom: 4
    };

    // Interaction state
    this.isDragging = false;
    this.isPanning = false;
    this.isConnecting = false;
    this.dragOffset = { x: 0, y: 0 };
    this.dragStartPos = { x: 0, y: 0 };
    this.connectionStart = null;
    this.connectionEnd = { x: 0, y: 0 };

    // ID counters
    this.nextNodeId = 1;
    this.nextConnectionId = 1;

    // Callbacks
    this.onNodeSelect = this.options.onNodeSelect || function() {};
    this.onConnectionCreate = this.options.onConnectionCreate || function() {};
    this.onChange = this.options.onChange || function() {};

    // Initialize
    this._init();
}

/**
 * Initialize the editor
 */
GraphEditor.prototype._init = function() {
    if (!this.canvas) {
        console.error('Graph canvas not found');
        return;
    }

    this._setupCanvas();
    this._bindEvents();
    this._render();
};

/**
 * Set up canvas size
 */
GraphEditor.prototype._setupCanvas = function() {
    var container = this.canvas.parentElement;
    this.canvas.width = container.clientWidth;
    this.canvas.height = container.clientHeight;

    // Handle resize
    var self = this;
    window.addEventListener('resize', debounce(function() {
        self.canvas.width = container.clientWidth;
        self.canvas.height = container.clientHeight;
        self._render();
    }, 100));
};

/**
 * Bind mouse and keyboard events
 */
GraphEditor.prototype._bindEvents = function() {
    var self = this;
    var canvas = this.canvas;

    // Mouse events on canvas container
    var container = canvas.parentElement;

    container.addEventListener('mousedown', function(e) {
        self._handleMouseDown(e);
    });

    container.addEventListener('mousemove', function(e) {
        self._handleMouseMove(e);
    });

    container.addEventListener('mouseup', function(e) {
        self._handleMouseUp(e);
    });

    container.addEventListener('wheel', function(e) {
        self._handleWheel(e);
    });

    // Keyboard
    document.addEventListener('keydown', function(e) {
        self._handleKeyDown(e);
    });

    // Context menu
    container.addEventListener('contextmenu', function(e) {
        e.preventDefault();
    });
};

/**
 * Handle mouse down
 */
GraphEditor.prototype._handleMouseDown = function(e) {
    var pos = this._getMousePos(e);
    var graphPos = this._screenToGraph(pos.x, pos.y);

    if (e.button === 1 || (e.button === 0 && e.shiftKey)) {
        // Middle click or shift+left click - start panning
        this.isPanning = true;
        this.dragStartPos = { x: pos.x, y: pos.y };
        return;
    }

    if (e.button === 0) {
        // Left click
        var node = this._getNodeAtPosition(graphPos.x, graphPos.y);
        var port = node ? this._getPortAtPosition(node, graphPos.x, graphPos.y) : null;

        if (port) {
            // Start connection
            this.isConnecting = true;
            this.connectionStart = { node: node, port: port };
            this.connectionEnd = graphPos;
        } else if (node) {
            // Select/drag node
            if (!e.ctrlKey && !e.metaKey && !this.selectedNodes.has(node.id)) {
                this.clearSelection();
            }
            this.selectNode(node.id, true);
            this.isDragging = true;
            this.dragOffset = {
                x: graphPos.x - node.x,
                y: graphPos.y - node.y
            };
        } else {
            // Click on empty space - clear selection
            if (!e.ctrlKey && !e.metaKey) {
                this.clearSelection();
            }
        }
    }
};

/**
 * Handle mouse move
 */
GraphEditor.prototype._handleMouseMove = function(e) {
    var pos = this._getMousePos(e);
    var graphPos = this._screenToGraph(pos.x, pos.y);

    if (this.isPanning) {
        var dx = pos.x - this.dragStartPos.x;
        var dy = pos.y - this.dragStartPos.y;
        this.viewport.panX += dx;
        this.viewport.panY += dy;
        this.dragStartPos = { x: pos.x, y: pos.y };
        this._updateTransform();
    } else if (this.isDragging && this.selectedNodes.size > 0) {
        // Move selected nodes
        var self = this;
        this.selectedNodes.forEach(function(nodeId) {
            var node = self._getNodeById(nodeId);
            if (node) {
                node.x = graphPos.x - self.dragOffset.x;
                node.y = graphPos.y - self.dragOffset.y;
            }
        });
        this._updateNodePositions();
        this._renderConnections();
    } else if (this.isConnecting) {
        this.connectionEnd = graphPos;
        this._renderConnections();
    }
};

/**
 * Handle mouse up
 */
GraphEditor.prototype._handleMouseUp = function(e) {
    var pos = this._getMousePos(e);
    var graphPos = this._screenToGraph(pos.x, pos.y);

    if (this.isConnecting) {
        // Check if we ended on a valid port
        var node = this._getNodeAtPosition(graphPos.x, graphPos.y);
        var port = node ? this._getPortAtPosition(node, graphPos.x, graphPos.y) : null;

        if (port && node.id !== this.connectionStart.node.id) {
            // Create connection
            this.createConnection(
                this.connectionStart.node.id,
                this.connectionStart.port.id,
                node.id,
                port.id
            );
        }
    }

    this.isPanning = false;
    this.isDragging = false;
    this.isConnecting = false;
    this.connectionStart = null;

    this._renderConnections();
};

/**
 * Handle mouse wheel (zoom)
 */
GraphEditor.prototype._handleWheel = function(e) {
    e.preventDefault();

    var delta = e.deltaY > 0 ? 0.9 : 1.1;
    var newZoom = this.viewport.zoom * delta;
    newZoom = Math.max(this.viewport.minZoom, Math.min(this.viewport.maxZoom, newZoom));

    if (newZoom !== this.viewport.zoom) {
        // Zoom towards mouse position
        var pos = this._getMousePos(e);
        var graphBefore = this._screenToGraph(pos.x, pos.y);

        this.viewport.zoom = newZoom;

        var graphAfter = this._screenToGraph(pos.x, pos.y);
        this.viewport.panX += (graphAfter.x - graphBefore.x) * this.viewport.zoom;
        this.viewport.panY += (graphAfter.y - graphBefore.y) * this.viewport.zoom;

        this._updateTransform();
    }
};

/**
 * Handle keyboard input
 */
GraphEditor.prototype._handleKeyDown = function(e) {
    if (e.key === 'Delete' || e.key === 'Backspace') {
        this.deleteSelectedNodes();
    } else if (e.key === 'a' && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        this.selectAll();
    } else if (e.key === 'Escape') {
        this.clearSelection();
    }
};

/**
 * Get mouse position relative to canvas
 */
GraphEditor.prototype._getMousePos = function(e) {
    var rect = this.canvas.getBoundingClientRect();
    return {
        x: e.clientX - rect.left,
        y: e.clientY - rect.top
    };
};

/**
 * Convert screen coordinates to graph coordinates
 */
GraphEditor.prototype._screenToGraph = function(x, y) {
    return {
        x: (x - this.viewport.panX) / this.viewport.zoom,
        y: (y - this.viewport.panY) / this.viewport.zoom
    };
};

/**
 * Convert graph coordinates to screen coordinates
 */
GraphEditor.prototype._graphToScreen = function(x, y) {
    return {
        x: x * this.viewport.zoom + this.viewport.panX,
        y: y * this.viewport.zoom + this.viewport.panY
    };
};

/**
 * Get node at position
 */
GraphEditor.prototype._getNodeAtPosition = function(x, y) {
    for (var i = this.nodes.length - 1; i >= 0; i--) {
        var node = this.nodes[i];
        if (x >= node.x && x <= node.x + node.width &&
            y >= node.y && y <= node.y + node.height) {
            return node;
        }
    }
    return null;
};

/**
 * Get port at position
 */
GraphEditor.prototype._getPortAtPosition = function(node, x, y) {
    var portRadius = 6;

    // Check outputs
    var outputY = node.y + 30;
    for (var i = 0; i < (node.outputs || []).length; i++) {
        var port = node.outputs[i];
        var portX = node.x + node.width;
        var portY = outputY + i * 20;

        if (Math.sqrt(Math.pow(x - portX, 2) + Math.pow(y - portY, 2)) < portRadius * 2) {
            return port;
        }
    }

    // Check inputs
    var inputY = node.y + 30;
    for (var i = 0; i < (node.inputs || []).length; i++) {
        var port = node.inputs[i];
        var portX = node.x;
        var portY = inputY + i * 20;

        if (Math.sqrt(Math.pow(x - portX, 2) + Math.pow(y - portY, 2)) < portRadius * 2) {
            return port;
        }
    }

    return null;
};

/**
 * Get node by ID
 */
GraphEditor.prototype._getNodeById = function(id) {
    return this.nodes.find(function(n) { return n.id === id; });
};

/**
 * Get connection by ID
 */
GraphEditor.prototype._getConnectionById = function(id) {
    return this.connections.find(function(c) { return c.id === id; });
};

// ============================================================================
// Node Management
// ============================================================================

/**
 * Create a node at position
 */
GraphEditor.prototype.createNodeAt = function(type, x, y) {
    var graphPos = this._screenToGraph(x, y);

    var typeDef = this.nodeTypes[type] || {};
    var node = {
        id: 'node_' + (this.nextNodeId++),
        type: type,
        title: typeDef.title || type,
        x: graphPos.x,
        y: graphPos.y,
        width: 180,
        height: 80,
        inputs: [{ id: 'in', name: 'In', type: 'flow' }],
        outputs: [{ id: 'out', name: 'Out', type: 'flow' }],
        data: {},
        headerColor: typeDef.color || '#444444'
    };

    this.nodes.push(node);
    this._createNodeElement(node);
    this.onChange();

    return node;
};

/**
 * Create DOM element for node
 */
GraphEditor.prototype._createNodeElement = function(node) {
    if (!this.nodesContainer) return;

    var el = document.createElement('div');
    el.className = 'graph-node';
    el.setAttribute('data-id', node.id);
    el.style.left = node.x + 'px';
    el.style.top = node.y + 'px';
    el.style.width = node.width + 'px';

    // Header
    var header = document.createElement('div');
    header.className = 'graph-node-header';
    header.style.background = node.headerColor;
    header.textContent = node.title;
    el.appendChild(header);

    // Content
    var content = document.createElement('div');
    content.className = 'graph-node-content';

    // Input ports
    var inputPorts = document.createElement('div');
    inputPorts.className = 'graph-node-ports inputs';
    (node.inputs || []).forEach(function(port) {
        var portEl = document.createElement('div');
        portEl.className = 'graph-port graph-port-input';
        portEl.innerHTML = '<div class="graph-port-dot" data-port="' + port.id + '"></div><span>' + port.name + '</span>';
        inputPorts.appendChild(portEl);
    });
    content.appendChild(inputPorts);

    // Output ports
    var outputPorts = document.createElement('div');
    outputPorts.className = 'graph-node-ports outputs';
    (node.outputs || []).forEach(function(port) {
        var portEl = document.createElement('div');
        portEl.className = 'graph-port graph-port-output';
        portEl.innerHTML = '<span>' + port.name + '</span><div class="graph-port-dot" data-port="' + port.id + '"></div>';
        outputPorts.appendChild(portEl);
    });
    content.appendChild(outputPorts);

    el.appendChild(content);
    this.nodesContainer.appendChild(el);

    return el;
};

/**
 * Update node element positions
 */
GraphEditor.prototype._updateNodePositions = function() {
    var self = this;
    this.nodes.forEach(function(node) {
        var el = self.nodesContainer.querySelector('[data-id="' + node.id + '"]');
        if (el) {
            el.style.left = node.x + 'px';
            el.style.top = node.y + 'px';
        }
    });
};

/**
 * Delete selected nodes
 */
GraphEditor.prototype.deleteSelectedNodes = function() {
    var self = this;

    this.selectedNodes.forEach(function(nodeId) {
        // Remove connections
        self.connections = self.connections.filter(function(conn) {
            return conn.sourceNodeId !== nodeId && conn.targetNodeId !== nodeId;
        });

        // Remove node
        self.nodes = self.nodes.filter(function(n) { return n.id !== nodeId; });

        // Remove DOM element
        var el = self.nodesContainer.querySelector('[data-id="' + nodeId + '"]');
        if (el) el.remove();
    });

    this.selectedNodes.clear();
    this._renderConnections();
    this.onChange();
};

// ============================================================================
// Connection Management
// ============================================================================

/**
 * Create a connection
 */
GraphEditor.prototype.createConnection = function(sourceNodeId, sourcePortId, targetNodeId, targetPortId) {
    // Check if connection already exists
    var exists = this.connections.some(function(c) {
        return c.sourceNodeId === sourceNodeId &&
               c.sourcePortId === sourcePortId &&
               c.targetNodeId === targetNodeId &&
               c.targetPortId === targetPortId;
    });

    if (exists) return null;

    var conn = {
        id: 'conn_' + (this.nextConnectionId++),
        sourceNodeId: sourceNodeId,
        sourcePortId: sourcePortId,
        targetNodeId: targetNodeId,
        targetPortId: targetPortId
    };

    this.connections.push(conn);
    this._renderConnections();
    this.onConnectionCreate(conn);
    this.onChange();

    return conn;
};

/**
 * Render connections
 */
GraphEditor.prototype._renderConnections = function() {
    if (!this.connectionsContainer) return;

    var self = this;
    var svg = this.connectionsContainer;

    // Clear existing
    svg.innerHTML = '';

    // Draw existing connections
    this.connections.forEach(function(conn) {
        var sourceNode = self._getNodeById(conn.sourceNodeId);
        var targetNode = self._getNodeById(conn.targetNodeId);

        if (!sourceNode || !targetNode) return;

        var startPos = self._getPortPosition(sourceNode, conn.sourcePortId, false);
        var endPos = self._getPortPosition(targetNode, conn.targetPortId, true);

        self._drawConnection(svg, startPos.x, startPos.y, endPos.x, endPos.y,
                            self.selectedConnections.has(conn.id));
    });

    // Draw in-progress connection
    if (this.isConnecting && this.connectionStart) {
        var startNode = this.connectionStart.node;
        var startPort = this.connectionStart.port;
        var isInput = startNode.inputs && startNode.inputs.indexOf(startPort) !== -1;
        var startPos = this._getPortPosition(startNode, startPort.id, isInput);

        this._drawConnection(svg, startPos.x, startPos.y,
                            this.connectionEnd.x, this.connectionEnd.y, false, true);
    }
};

/**
 * Get port position for a node
 */
GraphEditor.prototype._getPortPosition = function(node, portId, isInput) {
    var portIndex = 0;
    var ports = isInput ? node.inputs : node.outputs;

    for (var i = 0; i < ports.length; i++) {
        if (ports[i].id === portId) {
            portIndex = i;
            break;
        }
    }

    return {
        x: isInput ? node.x : node.x + node.width,
        y: node.y + 40 + portIndex * 20
    };
};

/**
 * Draw a bezier connection curve
 */
GraphEditor.prototype._drawConnection = function(svg, x1, y1, x2, y2, selected, inProgress) {
    var path = document.createElementNS('http://www.w3.org/2000/svg', 'path');

    // Calculate control points for bezier
    var dx = Math.abs(x2 - x1);
    var cp = Math.max(50, dx * 0.5);

    var d = 'M ' + x1 + ' ' + y1 +
            ' C ' + (x1 + cp) + ' ' + y1 +
            ', ' + (x2 - cp) + ' ' + y2 +
            ', ' + x2 + ' ' + y2;

    path.setAttribute('d', d);
    path.setAttribute('class', 'graph-connection' + (selected ? ' selected' : ''));

    if (inProgress) {
        path.style.strokeDasharray = '5,5';
        path.style.opacity = '0.5';
    }

    svg.appendChild(path);
};

// ============================================================================
// Selection
// ============================================================================

/**
 * Select a node
 */
GraphEditor.prototype.selectNode = function(nodeId, addToSelection) {
    if (!addToSelection) {
        this.clearSelection();
    }

    this.selectedNodes.add(nodeId);

    // Update visual
    var el = this.nodesContainer.querySelector('[data-id="' + nodeId + '"]');
    if (el) {
        addClass(el, 'selected');
    }

    var node = this._getNodeById(nodeId);
    if (node) {
        this.onNodeSelect(node);
    }
};

/**
 * Clear selection
 */
GraphEditor.prototype.clearSelection = function() {
    var self = this;

    this.selectedNodes.forEach(function(nodeId) {
        var el = self.nodesContainer.querySelector('[data-id="' + nodeId + '"]');
        if (el) {
            removeClass(el, 'selected');
        }
    });

    this.selectedNodes.clear();
    this.selectedConnections.clear();
};

/**
 * Select all nodes
 */
GraphEditor.prototype.selectAll = function() {
    var self = this;
    this.nodes.forEach(function(node) {
        self.selectedNodes.add(node.id);
        var el = self.nodesContainer.querySelector('[data-id="' + node.id + '"]');
        if (el) {
            addClass(el, 'selected');
        }
    });
};

// ============================================================================
// Viewport
// ============================================================================

/**
 * Update transform based on viewport
 */
GraphEditor.prototype._updateTransform = function() {
    if (this.nodesContainer) {
        this.nodesContainer.style.transform =
            'translate(' + this.viewport.panX + 'px, ' + this.viewport.panY + 'px) ' +
            'scale(' + this.viewport.zoom + ')';
        this.nodesContainer.style.transformOrigin = '0 0';
    }

    this._render();
    this._renderConnections();
};

/**
 * Zoom
 */
GraphEditor.prototype.zoom = function(factor) {
    var newZoom = this.viewport.zoom * factor;
    newZoom = Math.max(this.viewport.minZoom, Math.min(this.viewport.maxZoom, newZoom));
    this.viewport.zoom = newZoom;
    this._updateTransform();
};

/**
 * Get current zoom level
 */
GraphEditor.prototype.getZoom = function() {
    return this.viewport.zoom;
};

/**
 * Zoom to fit all nodes
 */
GraphEditor.prototype.zoomToFit = function() {
    if (this.nodes.length === 0) return;

    var minX = Infinity, minY = Infinity;
    var maxX = -Infinity, maxY = -Infinity;

    this.nodes.forEach(function(node) {
        minX = Math.min(minX, node.x);
        minY = Math.min(minY, node.y);
        maxX = Math.max(maxX, node.x + node.width);
        maxY = Math.max(maxY, node.y + node.height);
    });

    var graphWidth = maxX - minX;
    var graphHeight = maxY - minY;

    var zoomX = this.canvas.width / (graphWidth + 100);
    var zoomY = this.canvas.height / (graphHeight + 100);
    var newZoom = Math.min(zoomX, zoomY, 1);

    this.viewport.zoom = Math.max(this.viewport.minZoom, Math.min(this.viewport.maxZoom, newZoom));
    this.viewport.panX = this.canvas.width / 2 - (minX + graphWidth / 2) * this.viewport.zoom;
    this.viewport.panY = this.canvas.height / 2 - (minY + graphHeight / 2) * this.viewport.zoom;

    this._updateTransform();
};

// ============================================================================
// Auto Layout
// ============================================================================

/**
 * Auto arrange nodes
 */
GraphEditor.prototype.autoArrange = function(algorithm) {
    if (algorithm === 'hierarchical') {
        this._layoutHierarchical();
    } else if (algorithm === 'grid') {
        this._layoutGrid();
    }

    this._updateNodePositions();
    this._renderConnections();
    this.onChange();
};

/**
 * Hierarchical layout
 */
GraphEditor.prototype._layoutHierarchical = function() {
    var self = this;

    // Find root nodes
    var hasIncoming = new Set();
    this.connections.forEach(function(conn) {
        hasIncoming.add(conn.targetNodeId);
    });

    var roots = this.nodes.filter(function(n) {
        return !hasIncoming.has(n.id);
    });

    if (roots.length === 0 && this.nodes.length > 0) {
        roots = [this.nodes[0]];
    }

    // BFS to assign levels
    var levels = {};
    var queue = roots.map(function(r) { return { node: r, level: 0 }; });

    while (queue.length > 0) {
        var item = queue.shift();
        if (levels[item.node.id] !== undefined) continue;
        levels[item.node.id] = item.level;

        self.connections.forEach(function(conn) {
            if (conn.sourceNodeId === item.node.id) {
                var targetNode = self._getNodeById(conn.targetNodeId);
                if (targetNode && levels[targetNode.id] === undefined) {
                    queue.push({ node: targetNode, level: item.level + 1 });
                }
            }
        });
    }

    // Group by level
    var levelGroups = {};
    this.nodes.forEach(function(node) {
        var level = levels[node.id] || 0;
        if (!levelGroups[level]) levelGroups[level] = [];
        levelGroups[level].push(node);
    });

    // Position
    var levelSpacing = 150;
    var nodeSpacing = 200;

    Object.keys(levelGroups).forEach(function(level) {
        var nodes = levelGroups[level];
        var startX = -(nodes.length - 1) * nodeSpacing / 2;

        nodes.forEach(function(node, i) {
            node.x = startX + i * nodeSpacing;
            node.y = parseInt(level) * levelSpacing;
        });
    });
};

/**
 * Grid layout
 */
GraphEditor.prototype._layoutGrid = function() {
    var cols = Math.ceil(Math.sqrt(this.nodes.length));
    var spacing = 250;

    this.nodes.forEach(function(node, i) {
        var row = Math.floor(i / cols);
        var col = i % cols;
        node.x = col * spacing;
        node.y = row * spacing;
    });
};

// ============================================================================
// Data Management
// ============================================================================

/**
 * Update selected node's data
 */
GraphEditor.prototype.updateSelectedNodeData = function(property, value) {
    var self = this;
    this.selectedNodes.forEach(function(nodeId) {
        var node = self._getNodeById(nodeId);
        if (node) {
            if (!node.data) node.data = {};
            node.data[property] = value;
        }
    });
    this.onChange();
};

/**
 * Load graph from data
 */
GraphEditor.prototype.loadGraph = function(data) {
    this.nodes = [];
    this.connections = [];

    if (this.nodesContainer) {
        this.nodesContainer.innerHTML = '';
    }

    var self = this;

    if (data.nodes) {
        data.nodes.forEach(function(nodeData) {
            var typeDef = self.nodeTypes[nodeData.type] || {};
            var node = {
                id: nodeData.id,
                type: nodeData.type,
                title: nodeData.title || typeDef.title || nodeData.type,
                x: nodeData.x || 0,
                y: nodeData.y || 0,
                width: nodeData.width || 180,
                height: nodeData.height || 80,
                inputs: nodeData.inputs || [{ id: 'in', name: 'In', type: 'flow' }],
                outputs: nodeData.outputs || [{ id: 'out', name: 'Out', type: 'flow' }],
                data: nodeData.data || {},
                headerColor: typeDef.color || '#444444'
            };

            self.nodes.push(node);
            self._createNodeElement(node);

            // Update ID counter
            var idNum = parseInt(node.id.replace('node_', ''));
            if (!isNaN(idNum) && idNum >= self.nextNodeId) {
                self.nextNodeId = idNum + 1;
            }
        });
    }

    if (data.connections) {
        data.connections.forEach(function(connData) {
            var conn = {
                id: connData.id || 'conn_' + (self.nextConnectionId++),
                sourceNodeId: connData.sourceNodeId,
                sourcePortId: connData.sourcePortId,
                targetNodeId: connData.targetNodeId,
                targetPortId: connData.targetPortId
            };
            self.connections.push(conn);
        });
    }

    if (data.viewport) {
        this.viewport.panX = data.viewport.panX || 0;
        this.viewport.panY = data.viewport.panY || 0;
        this.viewport.zoom = data.viewport.zoom || 1;
    }

    this._updateTransform();
    this._renderConnections();
};

/**
 * Export graph to data
 */
GraphEditor.prototype.exportGraph = function() {
    return {
        nodes: this.nodes.map(function(n) {
            return {
                id: n.id,
                type: n.type,
                title: n.title,
                x: n.x,
                y: n.y,
                width: n.width,
                height: n.height,
                data: n.data
            };
        }),
        connections: this.connections.map(function(c) {
            return {
                id: c.id,
                sourceNodeId: c.sourceNodeId,
                sourcePortId: c.sourcePortId,
                targetNodeId: c.targetNodeId,
                targetPortId: c.targetPortId
            };
        }),
        viewport: {
            panX: this.viewport.panX,
            panY: this.viewport.panY,
            zoom: this.viewport.zoom
        }
    };
};

// ============================================================================
// Rendering
// ============================================================================

/**
 * Main render function
 */
GraphEditor.prototype._render = function() {
    if (!this.ctx) return;

    var ctx = this.ctx;
    var width = this.canvas.width;
    var height = this.canvas.height;

    // Clear
    ctx.clearRect(0, 0, width, height);

    // Draw grid
    this._drawGrid();
};

/**
 * Draw background grid
 */
GraphEditor.prototype._drawGrid = function() {
    var ctx = this.ctx;
    var gridSize = 20 * this.viewport.zoom;

    ctx.strokeStyle = 'rgba(255, 255, 255, 0.03)';
    ctx.lineWidth = 1;

    var offsetX = this.viewport.panX % gridSize;
    var offsetY = this.viewport.panY % gridSize;

    // Vertical lines
    for (var x = offsetX; x < this.canvas.width; x += gridSize) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, this.canvas.height);
        ctx.stroke();
    }

    // Horizontal lines
    for (var y = offsetY; y < this.canvas.height; y += gridSize) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(this.canvas.width, y);
        ctx.stroke();
    }
};

// Export
window.GraphEditor = GraphEditor;
