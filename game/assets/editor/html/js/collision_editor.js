/**
 * Nova3D/Vehement2 Collision Shape Editor
 * Visual collision shape editor with box, sphere, capsule, convex hull tools
 * Includes transform gizmos and JSON export
 */

// ============================================================================
// Collision Editor Class
// ============================================================================

class CollisionEditor {
    constructor(scene, options = {}) {
        this.scene = scene;
        this.options = Object.assign({
            defaultColor: 0x00ff00,
            selectedColor: 0x007acc,
            wireframeOpacity: 0.5,
            solidOpacity: 0.2,
            showLabels: true,
            snapToGrid: true,
            gridSize: 0.25
        }, options);

        this.shapes = [];
        this.selectedShape = null;
        this.currentTool = 'select';
        this.transformMode = 'translate';
        this.isDrawing = false;
        this.drawStart = null;

        this.transformControls = null;
        this.gizmo = null;

        this.onShapeSelect = () => {};
        this.onShapeChange = () => {};
        this.onShapeAdd = () => {};
        this.onShapeRemove = () => {};

        this._initMaterials();
        this._initTransformControls();
    }

    _initMaterials() {
        this.materials = {
            box: {
                wireframe: new THREE.MeshBasicMaterial({
                    color: 0x00ff00,
                    wireframe: true,
                    transparent: true,
                    opacity: this.options.wireframeOpacity
                }),
                solid: new THREE.MeshBasicMaterial({
                    color: 0x00ff00,
                    transparent: true,
                    opacity: this.options.solidOpacity,
                    side: THREE.DoubleSide
                })
            },
            sphere: {
                wireframe: new THREE.MeshBasicMaterial({
                    color: 0x0088ff,
                    wireframe: true,
                    transparent: true,
                    opacity: this.options.wireframeOpacity
                }),
                solid: new THREE.MeshBasicMaterial({
                    color: 0x0088ff,
                    transparent: true,
                    opacity: this.options.solidOpacity,
                    side: THREE.DoubleSide
                })
            },
            capsule: {
                wireframe: new THREE.MeshBasicMaterial({
                    color: 0xff8800,
                    wireframe: true,
                    transparent: true,
                    opacity: this.options.wireframeOpacity
                }),
                solid: new THREE.MeshBasicMaterial({
                    color: 0xff8800,
                    transparent: true,
                    opacity: this.options.solidOpacity,
                    side: THREE.DoubleSide
                })
            },
            cylinder: {
                wireframe: new THREE.MeshBasicMaterial({
                    color: 0xff00ff,
                    wireframe: true,
                    transparent: true,
                    opacity: this.options.wireframeOpacity
                }),
                solid: new THREE.MeshBasicMaterial({
                    color: 0xff00ff,
                    transparent: true,
                    opacity: this.options.solidOpacity,
                    side: THREE.DoubleSide
                })
            },
            convexHull: {
                wireframe: new THREE.MeshBasicMaterial({
                    color: 0xffff00,
                    wireframe: true,
                    transparent: true,
                    opacity: this.options.wireframeOpacity
                }),
                solid: new THREE.MeshBasicMaterial({
                    color: 0xffff00,
                    transparent: true,
                    opacity: this.options.solidOpacity,
                    side: THREE.DoubleSide
                })
            },
            selected: new THREE.MeshBasicMaterial({
                color: this.options.selectedColor,
                wireframe: true,
                transparent: true,
                opacity: 0.8
            })
        };
    }

    _initTransformControls() {
        if (this.scene.viewerScene && this.scene.viewerScene.transformControls) {
            this.transformControls = this.scene.viewerScene.transformControls;
        } else if (typeof THREE.TransformControls !== 'undefined' && this.scene.camera && this.scene.renderer) {
            this.transformControls = new THREE.TransformControls(
                this.scene.camera,
                this.scene.renderer.domElement
            );
            this.scene.scene.add(this.transformControls);

            this.transformControls.addEventListener('change', () => {
                if (this.selectedShape) {
                    this._updateShapeData(this.selectedShape);
                    this.onShapeChange(this.selectedShape);
                }
            });

            this.transformControls.addEventListener('dragging-changed', (event) => {
                if (this.scene.controls) {
                    this.scene.controls.enabled = !event.value;
                }
            });
        }
    }

    // ========================================================================
    // Tool Management
    // ========================================================================

    setTool(tool) {
        this.currentTool = tool;
        this.isDrawing = false;
        this.drawStart = null;

        if (tool === 'select' && this.transformControls && this.selectedShape) {
            this.transformControls.attach(this.selectedShape.mesh);
        } else if (this.transformControls) {
            this.transformControls.detach();
        }
    }

    setTransformMode(mode) {
        this.transformMode = mode;
        if (this.transformControls) {
            this.transformControls.setMode(mode);
        }
    }

    // ========================================================================
    // Shape Creation
    // ========================================================================

    createBox(params = {}) {
        const config = Object.assign({
            width: 1,
            height: 1,
            depth: 1,
            position: { x: 0, y: 0.5, z: 0 },
            rotation: { x: 0, y: 0, z: 0 },
            name: 'Box_' + (this.shapes.length + 1)
        }, params);

        const geometry = new THREE.BoxGeometry(config.width, config.height, config.depth);
        const wireframeMesh = new THREE.Mesh(geometry.clone(), this.materials.box.wireframe.clone());
        const solidMesh = new THREE.Mesh(geometry, this.materials.box.solid.clone());

        const group = new THREE.Group();
        group.add(wireframeMesh);
        group.add(solidMesh);
        group.position.set(config.position.x, config.position.y, config.position.z);
        group.rotation.set(config.rotation.x, config.rotation.y, config.rotation.z);

        const shape = {
            id: 'shape_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9),
            type: 'box',
            name: config.name,
            mesh: group,
            wireframe: wireframeMesh,
            solid: solidMesh,
            data: {
                type: 'box',
                size: { x: config.width, y: config.height, z: config.depth },
                position: { ...config.position },
                rotation: { ...config.rotation }
            }
        };

        this._addShape(shape);
        return shape;
    }

    createSphere(params = {}) {
        const config = Object.assign({
            radius: 0.5,
            position: { x: 0, y: 0.5, z: 0 },
            name: 'Sphere_' + (this.shapes.length + 1)
        }, params);

        const geometry = new THREE.SphereGeometry(config.radius, 16, 16);
        const wireframeMesh = new THREE.Mesh(geometry.clone(), this.materials.sphere.wireframe.clone());
        const solidMesh = new THREE.Mesh(geometry, this.materials.sphere.solid.clone());

        const group = new THREE.Group();
        group.add(wireframeMesh);
        group.add(solidMesh);
        group.position.set(config.position.x, config.position.y, config.position.z);

        const shape = {
            id: 'shape_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9),
            type: 'sphere',
            name: config.name,
            mesh: group,
            wireframe: wireframeMesh,
            solid: solidMesh,
            data: {
                type: 'sphere',
                radius: config.radius,
                position: { ...config.position }
            }
        };

        this._addShape(shape);
        return shape;
    }

    createCapsule(params = {}) {
        const config = Object.assign({
            radius: 0.3,
            height: 1.5,
            position: { x: 0, y: 0.75, z: 0 },
            name: 'Capsule_' + (this.shapes.length + 1)
        }, params);

        const cylinderHeight = config.height - config.radius * 2;
        const geometry = new THREE.CapsuleGeometry(config.radius, cylinderHeight, 8, 16);
        const wireframeMesh = new THREE.Mesh(geometry.clone(), this.materials.capsule.wireframe.clone());
        const solidMesh = new THREE.Mesh(geometry, this.materials.capsule.solid.clone());

        const group = new THREE.Group();
        group.add(wireframeMesh);
        group.add(solidMesh);
        group.position.set(config.position.x, config.position.y, config.position.z);

        const shape = {
            id: 'shape_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9),
            type: 'capsule',
            name: config.name,
            mesh: group,
            wireframe: wireframeMesh,
            solid: solidMesh,
            data: {
                type: 'capsule',
                radius: config.radius,
                height: config.height,
                position: { ...config.position }
            }
        };

        this._addShape(shape);
        return shape;
    }

    createCylinder(params = {}) {
        const config = Object.assign({
            radius: 0.5,
            height: 1,
            position: { x: 0, y: 0.5, z: 0 },
            name: 'Cylinder_' + (this.shapes.length + 1)
        }, params);

        const geometry = new THREE.CylinderGeometry(config.radius, config.radius, config.height, 16);
        const wireframeMesh = new THREE.Mesh(geometry.clone(), this.materials.cylinder.wireframe.clone());
        const solidMesh = new THREE.Mesh(geometry, this.materials.cylinder.solid.clone());

        const group = new THREE.Group();
        group.add(wireframeMesh);
        group.add(solidMesh);
        group.position.set(config.position.x, config.position.y, config.position.z);

        const shape = {
            id: 'shape_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9),
            type: 'cylinder',
            name: config.name,
            mesh: group,
            wireframe: wireframeMesh,
            solid: solidMesh,
            data: {
                type: 'cylinder',
                radius: config.radius,
                height: config.height,
                position: { ...config.position }
            }
        };

        this._addShape(shape);
        return shape;
    }

    createConvexHull(params = {}) {
        const config = Object.assign({
            points: [
                { x: 0, y: 0, z: 0 },
                { x: 1, y: 0, z: 0 },
                { x: 0.5, y: 1, z: 0 },
                { x: 0.5, y: 0.5, z: 1 }
            ],
            position: { x: 0, y: 0, z: 0 },
            name: 'ConvexHull_' + (this.shapes.length + 1)
        }, params);

        const points = config.points.map(p => new THREE.Vector3(p.x, p.y, p.z));

        let geometry;
        if (typeof THREE.ConvexGeometry !== 'undefined') {
            geometry = new THREE.ConvexGeometry(points);
        } else {
            // Fallback to simple buffer geometry
            geometry = new THREE.BufferGeometry().setFromPoints(points);
        }

        const wireframeMesh = new THREE.Mesh(geometry.clone(), this.materials.convexHull.wireframe.clone());
        const solidMesh = new THREE.Mesh(geometry, this.materials.convexHull.solid.clone());

        const group = new THREE.Group();
        group.add(wireframeMesh);
        group.add(solidMesh);
        group.position.set(config.position.x, config.position.y, config.position.z);

        const shape = {
            id: 'shape_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9),
            type: 'convexHull',
            name: config.name,
            mesh: group,
            wireframe: wireframeMesh,
            solid: solidMesh,
            data: {
                type: 'convexHull',
                points: config.points,
                position: { ...config.position }
            }
        };

        this._addShape(shape);
        return shape;
    }

    _addShape(shape) {
        this.shapes.push(shape);
        if (this.scene.scene) {
            this.scene.scene.add(shape.mesh);
        } else {
            this.scene.add(shape.mesh);
        }
        this.onShapeAdd(shape);
    }

    // ========================================================================
    // Shape Selection and Manipulation
    // ========================================================================

    selectShape(shape) {
        this.deselectShape();

        this.selectedShape = shape;
        if (shape) {
            shape.wireframe.material = this.materials.selected.clone();

            if (this.transformControls && this.currentTool === 'select') {
                this.transformControls.attach(shape.mesh);
            }

            this.onShapeSelect(shape);
        }
    }

    selectShapeById(id) {
        const shape = this.shapes.find(s => s.id === id);
        if (shape) {
            this.selectShape(shape);
        }
    }

    deselectShape() {
        if (this.selectedShape) {
            const shape = this.selectedShape;
            const materialType = this.materials[shape.type];
            if (materialType) {
                shape.wireframe.material = materialType.wireframe.clone();
            }

            if (this.transformControls) {
                this.transformControls.detach();
            }

            this.selectedShape = null;
            this.onShapeSelect(null);
        }
    }

    deleteShape(shape) {
        if (!shape) shape = this.selectedShape;
        if (!shape) return;

        const index = this.shapes.indexOf(shape);
        if (index > -1) {
            this.shapes.splice(index, 1);

            if (this.scene.scene) {
                this.scene.scene.remove(shape.mesh);
            } else {
                this.scene.remove(shape.mesh);
            }

            // Dispose geometry and materials
            shape.wireframe.geometry.dispose();
            shape.wireframe.material.dispose();
            shape.solid.geometry.dispose();
            shape.solid.material.dispose();

            if (this.selectedShape === shape) {
                this.selectedShape = null;
                if (this.transformControls) {
                    this.transformControls.detach();
                }
            }

            this.onShapeRemove(shape);
        }
    }

    deleteSelectedShape() {
        this.deleteShape(this.selectedShape);
    }

    duplicateShape(shape) {
        if (!shape) shape = this.selectedShape;
        if (!shape) return null;

        const newData = JSON.parse(JSON.stringify(shape.data));
        newData.position.x += 1;
        newData.name = shape.name + '_copy';

        let newShape;
        switch (shape.type) {
            case 'box':
                newShape = this.createBox({
                    width: newData.size.x,
                    height: newData.size.y,
                    depth: newData.size.z,
                    position: newData.position,
                    rotation: newData.rotation,
                    name: newData.name
                });
                break;
            case 'sphere':
                newShape = this.createSphere({
                    radius: newData.radius,
                    position: newData.position,
                    name: newData.name
                });
                break;
            case 'capsule':
                newShape = this.createCapsule({
                    radius: newData.radius,
                    height: newData.height,
                    position: newData.position,
                    name: newData.name
                });
                break;
            case 'cylinder':
                newShape = this.createCylinder({
                    radius: newData.radius,
                    height: newData.height,
                    position: newData.position,
                    name: newData.name
                });
                break;
            case 'convexHull':
                newShape = this.createConvexHull({
                    points: newData.points,
                    position: newData.position,
                    name: newData.name
                });
                break;
        }

        if (newShape) {
            this.selectShape(newShape);
        }

        return newShape;
    }

    // ========================================================================
    // Shape Updates
    // ========================================================================

    updateShapeSize(shape, size) {
        if (!shape) shape = this.selectedShape;
        if (!shape) return;

        // Update geometry based on type
        let newGeometry;
        switch (shape.type) {
            case 'box':
                newGeometry = new THREE.BoxGeometry(size.x, size.y, size.z);
                shape.data.size = { ...size };
                break;
            case 'sphere':
                newGeometry = new THREE.SphereGeometry(size.radius, 16, 16);
                shape.data.radius = size.radius;
                break;
            case 'capsule':
                const cylinderHeight = size.height - size.radius * 2;
                newGeometry = new THREE.CapsuleGeometry(size.radius, Math.max(0, cylinderHeight), 8, 16);
                shape.data.radius = size.radius;
                shape.data.height = size.height;
                break;
            case 'cylinder':
                newGeometry = new THREE.CylinderGeometry(size.radius, size.radius, size.height, 16);
                shape.data.radius = size.radius;
                shape.data.height = size.height;
                break;
        }

        if (newGeometry) {
            shape.wireframe.geometry.dispose();
            shape.wireframe.geometry = newGeometry.clone();
            shape.solid.geometry.dispose();
            shape.solid.geometry = newGeometry;
        }

        this.onShapeChange(shape);
    }

    updateShapePosition(shape, position) {
        if (!shape) shape = this.selectedShape;
        if (!shape) return;

        if (this.options.snapToGrid) {
            position.x = Math.round(position.x / this.options.gridSize) * this.options.gridSize;
            position.y = Math.round(position.y / this.options.gridSize) * this.options.gridSize;
            position.z = Math.round(position.z / this.options.gridSize) * this.options.gridSize;
        }

        shape.mesh.position.set(position.x, position.y, position.z);
        shape.data.position = { ...position };
        this.onShapeChange(shape);
    }

    updateShapeRotation(shape, rotation) {
        if (!shape) shape = this.selectedShape;
        if (!shape) return;

        shape.mesh.rotation.set(rotation.x, rotation.y, rotation.z);
        shape.data.rotation = { ...rotation };
        this.onShapeChange(shape);
    }

    _updateShapeData(shape) {
        shape.data.position = {
            x: shape.mesh.position.x,
            y: shape.mesh.position.y,
            z: shape.mesh.position.z
        };
        shape.data.rotation = {
            x: shape.mesh.rotation.x,
            y: shape.mesh.rotation.y,
            z: shape.mesh.rotation.z
        };
    }

    // ========================================================================
    // Visibility
    // ========================================================================

    setShapeVisible(shape, visible) {
        if (!shape) return;
        shape.mesh.visible = visible;
    }

    setAllShapesVisible(visible) {
        this.shapes.forEach(shape => {
            shape.mesh.visible = visible;
        });
    }

    setWireframeMode(wireframe) {
        this.shapes.forEach(shape => {
            shape.wireframe.visible = wireframe;
            shape.solid.visible = !wireframe;
        });
    }

    // ========================================================================
    // Raycast Selection
    // ========================================================================

    raycast(raycaster) {
        const intersects = [];

        this.shapes.forEach(shape => {
            const hits = raycaster.intersectObject(shape.mesh, true);
            if (hits.length > 0) {
                intersects.push({
                    shape: shape,
                    distance: hits[0].distance,
                    point: hits[0].point
                });
            }
        });

        intersects.sort((a, b) => a.distance - b.distance);
        return intersects;
    }

    selectAtPoint(raycaster) {
        const intersects = this.raycast(raycaster);
        if (intersects.length > 0) {
            this.selectShape(intersects[0].shape);
            return intersects[0].shape;
        } else {
            this.deselectShape();
            return null;
        }
    }

    // ========================================================================
    // Import/Export
    // ========================================================================

    exportToJSON() {
        return {
            version: 1,
            shapes: this.shapes.map(shape => ({
                id: shape.id,
                name: shape.name,
                ...shape.data
            }))
        };
    }

    importFromJSON(json) {
        // Clear existing shapes
        this.clearAll();

        if (!json || !json.shapes) return;

        json.shapes.forEach(shapeData => {
            switch (shapeData.type) {
                case 'box':
                    this.createBox({
                        width: shapeData.size?.x || 1,
                        height: shapeData.size?.y || 1,
                        depth: shapeData.size?.z || 1,
                        position: shapeData.position || { x: 0, y: 0, z: 0 },
                        rotation: shapeData.rotation || { x: 0, y: 0, z: 0 },
                        name: shapeData.name || 'Box'
                    });
                    break;
                case 'sphere':
                    this.createSphere({
                        radius: shapeData.radius || 0.5,
                        position: shapeData.position || { x: 0, y: 0, z: 0 },
                        name: shapeData.name || 'Sphere'
                    });
                    break;
                case 'capsule':
                    this.createCapsule({
                        radius: shapeData.radius || 0.3,
                        height: shapeData.height || 1.5,
                        position: shapeData.position || { x: 0, y: 0, z: 0 },
                        name: shapeData.name || 'Capsule'
                    });
                    break;
                case 'cylinder':
                    this.createCylinder({
                        radius: shapeData.radius || 0.5,
                        height: shapeData.height || 1,
                        position: shapeData.position || { x: 0, y: 0, z: 0 },
                        name: shapeData.name || 'Cylinder'
                    });
                    break;
                case 'convexHull':
                    this.createConvexHull({
                        points: shapeData.points || [],
                        position: shapeData.position || { x: 0, y: 0, z: 0 },
                        name: shapeData.name || 'ConvexHull'
                    });
                    break;
            }
        });
    }

    clearAll() {
        while (this.shapes.length > 0) {
            this.deleteShape(this.shapes[0]);
        }
        this.selectedShape = null;
    }

    // ========================================================================
    // Utility Methods
    // ========================================================================

    getShapes() {
        return this.shapes;
    }

    getShapeById(id) {
        return this.shapes.find(s => s.id === id);
    }

    getShapeByName(name) {
        return this.shapes.find(s => s.name === name);
    }

    getSelectedShape() {
        return this.selectedShape;
    }

    getBoundingBox() {
        if (this.shapes.length === 0) return null;

        const box = new THREE.Box3();

        this.shapes.forEach(shape => {
            const shapeBox = new THREE.Box3().setFromObject(shape.mesh);
            box.union(shapeBox);
        });

        return box;
    }

    centerShapes() {
        const box = this.getBoundingBox();
        if (!box) return;

        const center = box.getCenter(new THREE.Vector3());

        this.shapes.forEach(shape => {
            shape.mesh.position.sub(center);
            shape.data.position.x -= center.x;
            shape.data.position.y -= center.y;
            shape.data.position.z -= center.z;
        });
    }

    dispose() {
        this.clearAll();
        if (this.transformControls) {
            this.transformControls.dispose();
        }
    }
}

// ============================================================================
// Collision Editor UI Component
// ============================================================================

class CollisionEditorUI {
    constructor(containerId, editor) {
        this.container = document.getElementById(containerId);
        this.editor = editor;

        this._init();
        this._bindEvents();
    }

    _init() {
        if (!this.container) return;

        this.container.innerHTML = `
            <div class="collision-editor-panel">
                <div class="ce-toolbar">
                    <div class="ce-tool-group">
                        <button class="btn btn-small ce-tool active" data-tool="select" title="Select (V)">
                            <span>Select</span>
                        </button>
                        <button class="btn btn-small ce-tool" data-tool="box" title="Box (B)">
                            <span>Box</span>
                        </button>
                        <button class="btn btn-small ce-tool" data-tool="sphere" title="Sphere (S)">
                            <span>Sphere</span>
                        </button>
                        <button class="btn btn-small ce-tool" data-tool="capsule" title="Capsule (C)">
                            <span>Capsule</span>
                        </button>
                        <button class="btn btn-small ce-tool" data-tool="cylinder" title="Cylinder (Y)">
                            <span>Cylinder</span>
                        </button>
                    </div>
                    <div class="ce-tool-group">
                        <button class="btn btn-small ce-transform active" data-mode="translate" title="Move (G)">
                            Move
                        </button>
                        <button class="btn btn-small ce-transform" data-mode="rotate" title="Rotate (R)">
                            Rotate
                        </button>
                        <button class="btn btn-small ce-transform" data-mode="scale" title="Scale (S)">
                            Scale
                        </button>
                    </div>
                </div>

                <div class="ce-shapes-list">
                    <h4>Collision Shapes</h4>
                    <div class="ce-shapes-container"></div>
                    <button class="btn btn-small btn-add ce-add-shape">+ Add Shape</button>
                </div>

                <div class="ce-properties" style="display: none;">
                    <h4>Shape Properties</h4>
                    <div class="form-group">
                        <label>Name</label>
                        <input type="text" class="ce-prop-name">
                    </div>
                    <div class="form-group">
                        <label>Type</label>
                        <input type="text" class="ce-prop-type" readonly>
                    </div>
                    <div class="form-group">
                        <label>Position</label>
                        <div class="vector3-input">
                            <input type="number" class="ce-prop-pos-x" step="0.1" placeholder="X">
                            <input type="number" class="ce-prop-pos-y" step="0.1" placeholder="Y">
                            <input type="number" class="ce-prop-pos-z" step="0.1" placeholder="Z">
                        </div>
                    </div>
                    <div class="form-group ce-size-group">
                        <label>Size</label>
                        <div class="ce-size-fields"></div>
                    </div>
                    <div class="ce-actions">
                        <button class="btn btn-small ce-duplicate">Duplicate</button>
                        <button class="btn btn-small btn-danger ce-delete">Delete</button>
                    </div>
                </div>
            </div>
        `;

        this.toolButtons = this.container.querySelectorAll('.ce-tool');
        this.transformButtons = this.container.querySelectorAll('.ce-transform');
        this.shapesContainer = this.container.querySelector('.ce-shapes-container');
        this.propertiesPanel = this.container.querySelector('.ce-properties');
        this.sizeGroup = this.container.querySelector('.ce-size-group');
        this.sizeFields = this.container.querySelector('.ce-size-fields');
    }

    _bindEvents() {
        if (!this.container) return;

        // Tool selection
        this.toolButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                this.toolButtons.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                const tool = btn.dataset.tool;
                this.editor.setTool(tool);

                if (tool !== 'select') {
                    this._createShape(tool);
                }
            });
        });

        // Transform mode
        this.transformButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                this.transformButtons.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                this.editor.setTransformMode(btn.dataset.mode);
            });
        });

        // Add shape dropdown
        this.container.querySelector('.ce-add-shape').addEventListener('click', () => {
            const tool = document.querySelector('.ce-tool.active')?.dataset.tool || 'box';
            if (tool !== 'select') {
                this._createShape(tool);
            } else {
                this._createShape('box');
            }
        });

        // Property inputs
        this.container.querySelector('.ce-prop-name').addEventListener('change', (e) => {
            if (this.editor.selectedShape) {
                this.editor.selectedShape.name = e.target.value;
                this._updateShapesList();
            }
        });

        ['x', 'y', 'z'].forEach(axis => {
            this.container.querySelector(`.ce-prop-pos-${axis}`).addEventListener('change', () => {
                this._updateShapePosition();
            });
        });

        // Duplicate and delete
        this.container.querySelector('.ce-duplicate').addEventListener('click', () => {
            this.editor.duplicateShape();
            this._updateShapesList();
        });

        this.container.querySelector('.ce-delete').addEventListener('click', () => {
            this.editor.deleteSelectedShape();
            this._updateShapesList();
            this._hideProperties();
        });

        // Editor callbacks
        this.editor.onShapeSelect = (shape) => {
            this._updateShapesList();
            if (shape) {
                this._showProperties(shape);
            } else {
                this._hideProperties();
            }
        };

        this.editor.onShapeAdd = () => {
            this._updateShapesList();
        };

        this.editor.onShapeRemove = () => {
            this._updateShapesList();
        };

        this.editor.onShapeChange = () => {
            if (this.editor.selectedShape) {
                this._updatePropertiesValues(this.editor.selectedShape);
            }
        };

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

            switch (e.key.toLowerCase()) {
                case 'v':
                    this._setTool('select');
                    break;
                case 'b':
                    this._setTool('box');
                    break;
                case 'delete':
                case 'backspace':
                    this.editor.deleteSelectedShape();
                    break;
                case 'd':
                    if (e.ctrlKey) {
                        e.preventDefault();
                        this.editor.duplicateShape();
                    }
                    break;
                case 'g':
                    this._setTransformMode('translate');
                    break;
                case 'r':
                    this._setTransformMode('rotate');
                    break;
            }
        });
    }

    _setTool(tool) {
        this.toolButtons.forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tool === tool);
        });
        this.editor.setTool(tool);
    }

    _setTransformMode(mode) {
        this.transformButtons.forEach(btn => {
            btn.classList.toggle('active', btn.dataset.mode === mode);
        });
        this.editor.setTransformMode(mode);
    }

    _createShape(type) {
        switch (type) {
            case 'box':
                this.editor.createBox();
                break;
            case 'sphere':
                this.editor.createSphere();
                break;
            case 'capsule':
                this.editor.createCapsule();
                break;
            case 'cylinder':
                this.editor.createCylinder();
                break;
        }
        this._updateShapesList();
    }

    _updateShapesList() {
        if (!this.shapesContainer) return;

        this.shapesContainer.innerHTML = this.editor.shapes.map(shape => `
            <div class="ce-shape-item ${this.editor.selectedShape === shape ? 'selected' : ''}"
                 data-id="${shape.id}">
                <span class="ce-shape-type">${shape.type}</span>
                <span class="ce-shape-name">${shape.name}</span>
            </div>
        `).join('');

        this.shapesContainer.querySelectorAll('.ce-shape-item').forEach(item => {
            item.addEventListener('click', () => {
                this.editor.selectShapeById(item.dataset.id);
            });
        });
    }

    _showProperties(shape) {
        if (!this.propertiesPanel) return;

        this.propertiesPanel.style.display = 'block';
        this._updatePropertiesValues(shape);
        this._updateSizeFields(shape);
    }

    _hideProperties() {
        if (this.propertiesPanel) {
            this.propertiesPanel.style.display = 'none';
        }
    }

    _updatePropertiesValues(shape) {
        this.container.querySelector('.ce-prop-name').value = shape.name;
        this.container.querySelector('.ce-prop-type').value = shape.type;
        this.container.querySelector('.ce-prop-pos-x').value = shape.data.position.x.toFixed(2);
        this.container.querySelector('.ce-prop-pos-y').value = shape.data.position.y.toFixed(2);
        this.container.querySelector('.ce-prop-pos-z').value = shape.data.position.z.toFixed(2);

        // Update size fields based on type
        this._updateSizeFieldValues(shape);
    }

    _updateSizeFields(shape) {
        if (!this.sizeFields) return;

        let html = '';
        switch (shape.type) {
            case 'box':
                html = `
                    <div class="vector3-input">
                        <input type="number" class="ce-size-x" step="0.1" placeholder="W">
                        <input type="number" class="ce-size-y" step="0.1" placeholder="H">
                        <input type="number" class="ce-size-z" step="0.1" placeholder="D">
                    </div>
                `;
                break;
            case 'sphere':
                html = `
                    <input type="number" class="ce-size-radius" step="0.1" placeholder="Radius">
                `;
                break;
            case 'capsule':
            case 'cylinder':
                html = `
                    <div class="form-row">
                        <label>Radius</label>
                        <input type="number" class="ce-size-radius" step="0.1">
                    </div>
                    <div class="form-row">
                        <label>Height</label>
                        <input type="number" class="ce-size-height" step="0.1">
                    </div>
                `;
                break;
        }

        this.sizeFields.innerHTML = html;
        this._bindSizeInputs(shape);
        this._updateSizeFieldValues(shape);
    }

    _updateSizeFieldValues(shape) {
        switch (shape.type) {
            case 'box':
                const sizeX = this.container.querySelector('.ce-size-x');
                const sizeY = this.container.querySelector('.ce-size-y');
                const sizeZ = this.container.querySelector('.ce-size-z');
                if (sizeX) sizeX.value = shape.data.size?.x?.toFixed(2) || 1;
                if (sizeY) sizeY.value = shape.data.size?.y?.toFixed(2) || 1;
                if (sizeZ) sizeZ.value = shape.data.size?.z?.toFixed(2) || 1;
                break;
            case 'sphere':
                const radius = this.container.querySelector('.ce-size-radius');
                if (radius) radius.value = shape.data.radius?.toFixed(2) || 0.5;
                break;
            case 'capsule':
            case 'cylinder':
                const r = this.container.querySelector('.ce-size-radius');
                const h = this.container.querySelector('.ce-size-height');
                if (r) r.value = shape.data.radius?.toFixed(2) || 0.5;
                if (h) h.value = shape.data.height?.toFixed(2) || 1;
                break;
        }
    }

    _bindSizeInputs(shape) {
        const inputs = this.sizeFields.querySelectorAll('input');
        inputs.forEach(input => {
            input.addEventListener('change', () => {
                this._updateShapeSize(shape);
            });
        });
    }

    _updateShapePosition() {
        if (!this.editor.selectedShape) return;

        const x = parseFloat(this.container.querySelector('.ce-prop-pos-x').value) || 0;
        const y = parseFloat(this.container.querySelector('.ce-prop-pos-y').value) || 0;
        const z = parseFloat(this.container.querySelector('.ce-prop-pos-z').value) || 0;

        this.editor.updateShapePosition(this.editor.selectedShape, { x, y, z });
    }

    _updateShapeSize(shape) {
        let size = {};

        switch (shape.type) {
            case 'box':
                size = {
                    x: parseFloat(this.container.querySelector('.ce-size-x').value) || 1,
                    y: parseFloat(this.container.querySelector('.ce-size-y').value) || 1,
                    z: parseFloat(this.container.querySelector('.ce-size-z').value) || 1
                };
                break;
            case 'sphere':
                size = {
                    radius: parseFloat(this.container.querySelector('.ce-size-radius').value) || 0.5
                };
                break;
            case 'capsule':
            case 'cylinder':
                size = {
                    radius: parseFloat(this.container.querySelector('.ce-size-radius').value) || 0.5,
                    height: parseFloat(this.container.querySelector('.ce-size-height').value) || 1
                };
                break;
        }

        this.editor.updateShapeSize(shape, size);
    }
}

// ============================================================================
// Export
// ============================================================================

window.CollisionEditor = CollisionEditor;
window.CollisionEditorUI = CollisionEditorUI;
