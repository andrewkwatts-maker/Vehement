/**
 * Nova3D/Vehement2 Viewer Common Module
 * Shared utilities for all viewer HTML pages
 * Provides Three.js scene setup, JSON schema forms, Python editor integration, and bridge communication
 */

// ============================================================================
// Three.js Scene Setup
// ============================================================================

class ViewerScene {
    constructor(canvasId, options = {}) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) {
            console.error('Canvas not found:', canvasId);
            return;
        }

        this.options = Object.assign({
            backgroundColor: 0x1a1a1a,
            gridSize: 10,
            gridDivisions: 10,
            cameraPosition: { x: 5, y: 5, z: 5 },
            enableShadows: true,
            enableOrbitControls: true,
            enableTransformControls: false,
            antialias: true
        }, options);

        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.transformControls = null;
        this.clock = new THREE.Clock();
        this.mixer = null;
        this.model = null;
        this.collisionShapes = [];
        this.helpers = [];
        this.raycaster = new THREE.Raycaster();
        this.mouse = new THREE.Vector2();
        this.selectedObject = null;

        this._init();
    }

    _init() {
        // Create scene
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(this.options.backgroundColor);

        // Create camera
        const aspect = this.canvas.clientWidth / this.canvas.clientHeight;
        this.camera = new THREE.PerspectiveCamera(60, aspect, 0.1, 1000);
        this.camera.position.set(
            this.options.cameraPosition.x,
            this.options.cameraPosition.y,
            this.options.cameraPosition.z
        );

        // Create renderer
        this.renderer = new THREE.WebGLRenderer({
            canvas: this.canvas,
            antialias: this.options.antialias
        });
        this.renderer.setSize(this.canvas.clientWidth, this.canvas.clientHeight);
        this.renderer.setPixelRatio(window.devicePixelRatio);

        if (this.options.enableShadows) {
            this.renderer.shadowMap.enabled = true;
            this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        }

        // Add lights
        this._setupLights();

        // Add grid helper
        this._setupGrid();

        // Setup controls
        if (this.options.enableOrbitControls && typeof THREE.OrbitControls !== 'undefined') {
            this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
            this.controls.enableDamping = true;
            this.controls.dampingFactor = 0.05;
            this.controls.target.set(0, 1, 0);
            this.controls.update();
        }

        // Transform controls for editing
        if (this.options.enableTransformControls && typeof THREE.TransformControls !== 'undefined') {
            this.transformControls = new THREE.TransformControls(this.camera, this.renderer.domElement);
            this.transformControls.addEventListener('dragging-changed', (event) => {
                if (this.controls) {
                    this.controls.enabled = !event.value;
                }
            });
            this.scene.add(this.transformControls);
        }

        // Handle resize
        window.addEventListener('resize', () => this._onResize());

        // Mouse events
        this.canvas.addEventListener('click', (e) => this._onClick(e));
        this.canvas.addEventListener('mousemove', (e) => this._onMouseMove(e));

        // Start animation loop
        this._animate();
    }

    _setupLights() {
        // Ambient light
        const ambient = new THREE.AmbientLight(0x404040, 0.5);
        this.scene.add(ambient);

        // Main directional light
        const dirLight = new THREE.DirectionalLight(0xffffff, 1);
        dirLight.position.set(5, 10, 7);
        dirLight.castShadow = this.options.enableShadows;
        dirLight.shadow.mapSize.width = 2048;
        dirLight.shadow.mapSize.height = 2048;
        dirLight.shadow.camera.near = 0.5;
        dirLight.shadow.camera.far = 50;
        dirLight.shadow.camera.left = -10;
        dirLight.shadow.camera.right = 10;
        dirLight.shadow.camera.top = 10;
        dirLight.shadow.camera.bottom = -10;
        this.scene.add(dirLight);

        // Fill light
        const fillLight = new THREE.DirectionalLight(0x4488ff, 0.3);
        fillLight.position.set(-5, 5, -5);
        this.scene.add(fillLight);
    }

    _setupGrid() {
        const grid = new THREE.GridHelper(
            this.options.gridSize,
            this.options.gridDivisions,
            0x444444,
            0x333333
        );
        this.scene.add(grid);
        this.helpers.push(grid);

        // Ground plane for shadows
        const groundGeometry = new THREE.PlaneGeometry(this.options.gridSize, this.options.gridSize);
        const groundMaterial = new THREE.ShadowMaterial({ opacity: 0.3 });
        const ground = new THREE.Mesh(groundGeometry, groundMaterial);
        ground.rotation.x = -Math.PI / 2;
        ground.receiveShadow = true;
        this.scene.add(ground);
    }

    _onResize() {
        if (!this.canvas) return;

        const width = this.canvas.clientWidth;
        const height = this.canvas.clientHeight;

        this.camera.aspect = width / height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(width, height);
    }

    _onClick(event) {
        const rect = this.canvas.getBoundingClientRect();
        this.mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        this.mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

        this.raycaster.setFromCamera(this.mouse, this.camera);
        const intersects = this.raycaster.intersectObjects(this.collisionShapes, true);

        if (intersects.length > 0) {
            this.selectObject(intersects[0].object);
        } else {
            this.deselectObject();
        }
    }

    _onMouseMove(event) {
        const rect = this.canvas.getBoundingClientRect();
        this.mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        this.mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
    }

    _animate() {
        requestAnimationFrame(() => this._animate());

        const delta = this.clock.getDelta();

        if (this.mixer) {
            this.mixer.update(delta);
        }

        if (this.controls) {
            this.controls.update();
        }

        this.renderer.render(this.scene, this.camera);
    }

    // Public API

    loadModel(url, onProgress) {
        return new Promise((resolve, reject) => {
            if (!url) {
                resolve(null);
                return;
            }

            const loader = new THREE.GLTFLoader();

            loader.load(
                url,
                (gltf) => {
                    // Remove previous model
                    if (this.model) {
                        this.scene.remove(this.model);
                    }

                    this.model = gltf.scene;
                    this.model.traverse((child) => {
                        if (child.isMesh) {
                            child.castShadow = true;
                            child.receiveShadow = true;
                        }
                    });

                    // Center the model
                    const box = new THREE.Box3().setFromObject(this.model);
                    const center = box.getCenter(new THREE.Vector3());
                    this.model.position.sub(center);
                    this.model.position.y += box.getSize(new THREE.Vector3()).y / 2;

                    this.scene.add(this.model);

                    // Setup animation mixer
                    if (gltf.animations && gltf.animations.length > 0) {
                        this.mixer = new THREE.AnimationMixer(this.model);
                        this.animations = gltf.animations;
                    }

                    resolve(gltf);
                },
                (xhr) => {
                    if (onProgress) {
                        onProgress(xhr.loaded / xhr.total);
                    }
                },
                (error) => {
                    console.error('Error loading model:', error);
                    reject(error);
                }
            );
        });
    }

    playAnimation(name) {
        if (!this.mixer || !this.animations) return;

        const clip = THREE.AnimationClip.findByName(this.animations, name);
        if (clip) {
            this.mixer.stopAllAction();
            const action = this.mixer.clipAction(clip);
            action.play();
        }
    }

    getAnimationNames() {
        if (!this.animations) return [];
        return this.animations.map(a => a.name);
    }

    setModelScale(scale) {
        if (this.model) {
            this.model.scale.setScalar(scale);
        }
    }

    addCollisionShape(shape) {
        this.collisionShapes.push(shape);
        this.scene.add(shape);
    }

    removeCollisionShape(shape) {
        const index = this.collisionShapes.indexOf(shape);
        if (index > -1) {
            this.collisionShapes.splice(index, 1);
            this.scene.remove(shape);
        }
    }

    clearCollisionShapes() {
        this.collisionShapes.forEach(shape => {
            this.scene.remove(shape);
        });
        this.collisionShapes = [];
    }

    selectObject(object) {
        this.deselectObject();
        this.selectedObject = object;

        if (object.material) {
            object.userData.originalMaterial = object.material;
            object.material = object.material.clone();
            object.material.emissive = new THREE.Color(0x007acc);
            object.material.emissiveIntensity = 0.3;
        }

        if (this.transformControls) {
            this.transformControls.attach(object);
        }

        if (this.onObjectSelected) {
            this.onObjectSelected(object);
        }
    }

    deselectObject() {
        if (this.selectedObject) {
            if (this.selectedObject.userData.originalMaterial) {
                this.selectedObject.material = this.selectedObject.userData.originalMaterial;
            }

            if (this.transformControls) {
                this.transformControls.detach();
            }

            if (this.onObjectDeselected) {
                this.onObjectDeselected(this.selectedObject);
            }

            this.selectedObject = null;
        }
    }

    resetCamera() {
        this.camera.position.set(
            this.options.cameraPosition.x,
            this.options.cameraPosition.y,
            this.options.cameraPosition.z
        );
        if (this.controls) {
            this.controls.target.set(0, 1, 0);
            this.controls.update();
        }
    }

    setBackgroundColor(color) {
        this.scene.background = new THREE.Color(color);
    }

    dispose() {
        this.renderer.dispose();
        if (this.controls) {
            this.controls.dispose();
        }
        if (this.transformControls) {
            this.transformControls.dispose();
        }
    }
}

// ============================================================================
// Collision Shape Drawing Utilities
// ============================================================================

const CollisionShapeUtils = {
    materials: {
        wireframe: new THREE.MeshBasicMaterial({
            color: 0x00ff00,
            wireframe: true,
            transparent: true,
            opacity: 0.5
        }),
        solid: new THREE.MeshBasicMaterial({
            color: 0x00ff00,
            transparent: true,
            opacity: 0.2,
            side: THREE.DoubleSide
        }),
        selected: new THREE.MeshBasicMaterial({
            color: 0x007acc,
            transparent: true,
            opacity: 0.4
        })
    },

    createBox(width, height, depth, position = { x: 0, y: 0, z: 0 }) {
        const geometry = new THREE.BoxGeometry(width, height, depth);
        const mesh = new THREE.Mesh(geometry, this.materials.wireframe.clone());
        mesh.position.set(position.x, position.y, position.z);
        mesh.userData.shapeType = 'box';
        mesh.userData.dimensions = { width, height, depth };
        return mesh;
    },

    createSphere(radius, position = { x: 0, y: 0, z: 0 }) {
        const geometry = new THREE.SphereGeometry(radius, 16, 16);
        const mesh = new THREE.Mesh(geometry, this.materials.wireframe.clone());
        mesh.position.set(position.x, position.y, position.z);
        mesh.userData.shapeType = 'sphere';
        mesh.userData.dimensions = { radius };
        return mesh;
    },

    createCapsule(radius, height, position = { x: 0, y: 0, z: 0 }) {
        const geometry = new THREE.CapsuleGeometry(radius, height - radius * 2, 8, 16);
        const mesh = new THREE.Mesh(geometry, this.materials.wireframe.clone());
        mesh.position.set(position.x, position.y, position.z);
        mesh.userData.shapeType = 'capsule';
        mesh.userData.dimensions = { radius, height };
        return mesh;
    },

    createCylinder(radius, height, position = { x: 0, y: 0, z: 0 }) {
        const geometry = new THREE.CylinderGeometry(radius, radius, height, 16);
        const mesh = new THREE.Mesh(geometry, this.materials.wireframe.clone());
        mesh.position.set(position.x, position.y, position.z);
        mesh.userData.shapeType = 'cylinder';
        mesh.userData.dimensions = { radius, height };
        return mesh;
    },

    createConvexHull(points) {
        const geometry = new THREE.ConvexGeometry(
            points.map(p => new THREE.Vector3(p.x, p.y, p.z))
        );
        const mesh = new THREE.Mesh(geometry, this.materials.wireframe.clone());
        mesh.userData.shapeType = 'convexHull';
        mesh.userData.points = points;
        return mesh;
    },

    createFromConfig(config) {
        const pos = config.position || { x: 0, y: 0, z: 0 };

        switch (config.type) {
            case 'box':
                return this.createBox(
                    config.size?.x || 1,
                    config.size?.y || 1,
                    config.size?.z || 1,
                    pos
                );
            case 'sphere':
                return this.createSphere(config.radius || 0.5, pos);
            case 'capsule':
                return this.createCapsule(
                    config.radius || 0.5,
                    config.height || 2,
                    pos
                );
            case 'cylinder':
                return this.createCylinder(
                    config.radius || 0.5,
                    config.height || 2,
                    pos
                );
            default:
                console.warn('Unknown collision shape type:', config.type);
                return null;
        }
    },

    toConfig(mesh) {
        const config = {
            type: mesh.userData.shapeType,
            position: {
                x: mesh.position.x,
                y: mesh.position.y,
                z: mesh.position.z
            }
        };

        const dims = mesh.userData.dimensions;
        switch (mesh.userData.shapeType) {
            case 'box':
                config.size = { x: dims.width, y: dims.height, z: dims.depth };
                break;
            case 'sphere':
                config.radius = dims.radius;
                break;
            case 'capsule':
            case 'cylinder':
                config.radius = dims.radius;
                config.height = dims.height;
                break;
        }

        return config;
    }
};

// ============================================================================
// JSON Schema Form Generator
// ============================================================================

class SchemaFormGenerator {
    constructor(container, options = {}) {
        this.container = typeof container === 'string'
            ? document.querySelector(container)
            : container;
        this.schema = null;
        this.data = {};
        this.originalData = {};
        this.onChange = options.onChange || (() => {});
        this.onValidate = options.onValidate || (() => {});
        this.validationErrors = {};
        this.undoStack = [];
        this.redoStack = [];
    }

    setSchema(schema) {
        this.schema = schema;
    }

    setData(data) {
        this.data = JSON.parse(JSON.stringify(data));
        this.originalData = JSON.parse(JSON.stringify(data));
        this.undoStack = [];
        this.redoStack = [];
        this._populateForm();
    }

    getData() {
        this._collectFormData();
        return JSON.parse(JSON.stringify(this.data));
    }

    isDirty() {
        return JSON.stringify(this.data) !== JSON.stringify(this.originalData);
    }

    markClean() {
        this.originalData = JSON.parse(JSON.stringify(this.data));
    }

    undo() {
        if (this.undoStack.length === 0) return false;
        this.redoStack.push(JSON.parse(JSON.stringify(this.data)));
        this.data = this.undoStack.pop();
        this._populateForm();
        return true;
    }

    redo() {
        if (this.redoStack.length === 0) return false;
        this.undoStack.push(JSON.parse(JSON.stringify(this.data)));
        this.data = this.redoStack.pop();
        this._populateForm();
        return true;
    }

    _saveUndoState() {
        this.undoStack.push(JSON.parse(JSON.stringify(this.data)));
        this.redoStack = [];
        if (this.undoStack.length > 50) {
            this.undoStack.shift();
        }
    }

    _populateForm() {
        if (!this.container) return;
        const inputs = this.container.querySelectorAll('[data-path]');
        inputs.forEach(input => {
            const path = input.getAttribute('data-path');
            const value = this._getPath(this.data, path);
            this._setInputValue(input, value);
        });
    }

    _collectFormData() {
        if (!this.container) return;
        const inputs = this.container.querySelectorAll('[data-path]');
        inputs.forEach(input => {
            const path = input.getAttribute('data-path');
            const value = this._getInputValue(input);
            this._setPath(this.data, path, value);
        });
    }

    _getPath(obj, path) {
        if (!path) return obj;
        const parts = path.split('.');
        let current = obj;
        for (const part of parts) {
            if (current === null || current === undefined) return undefined;
            current = current[part];
        }
        return current;
    }

    _setPath(obj, path, value) {
        const parts = path.split('.');
        let current = obj;
        for (let i = 0; i < parts.length - 1; i++) {
            if (current[parts[i]] === undefined) {
                current[parts[i]] = {};
            }
            current = current[parts[i]];
        }
        current[parts[parts.length - 1]] = value;
    }

    _getInputValue(input) {
        const type = input.getAttribute('data-type') || input.type;
        switch (type) {
            case 'number':
            case 'integer':
            case 'float':
                const num = parseFloat(input.value);
                return isNaN(num) ? 0 : num;
            case 'checkbox':
            case 'boolean':
                return input.checked;
            case 'select-multiple':
                return Array.from(input.selectedOptions).map(o => o.value);
            default:
                return input.value;
        }
    }

    _setInputValue(input, value) {
        const type = input.getAttribute('data-type') || input.type;
        switch (type) {
            case 'checkbox':
            case 'boolean':
                input.checked = !!value;
                break;
            case 'color':
                input.value = value || '#ffffff';
                break;
            default:
                input.value = value !== undefined && value !== null ? value : '';
        }
    }

    generateField(name, config, path = '') {
        const fullPath = path ? `${path}.${name}` : name;
        let html = '<div class="form-group">';

        html += `<label for="${fullPath}">${config.label || name}`;
        if (config.required) {
            html += ' <span class="required">*</span>';
        }
        html += '</label>';

        if (config.description) {
            html += `<small class="help-text">${config.description}</small>`;
        }

        switch (config.type) {
            case 'string':
                html += `<input type="text" id="${fullPath}" data-path="${fullPath}">`;
                break;
            case 'number':
            case 'integer':
            case 'float':
                html += `<input type="number" id="${fullPath}" data-path="${fullPath}"
                    step="${config.type === 'integer' ? '1' : '0.01'}"
                    ${config.min !== undefined ? `min="${config.min}"` : ''}
                    ${config.max !== undefined ? `max="${config.max}"` : ''}>`;
                break;
            case 'boolean':
                html += `<input type="checkbox" id="${fullPath}" data-path="${fullPath}">`;
                break;
            case 'enum':
                html += `<select id="${fullPath}" data-path="${fullPath}">`;
                (config.options || []).forEach(opt => {
                    const val = typeof opt === 'object' ? opt.value : opt;
                    const label = typeof opt === 'object' ? opt.label : opt;
                    html += `<option value="${val}">${label}</option>`;
                });
                html += '</select>';
                break;
            case 'color':
                html += `<input type="color" id="${fullPath}" data-path="${fullPath}">`;
                break;
            case 'vector3':
                html += '<div class="vector3-input">';
                html += `<input type="number" data-path="${fullPath}.x" placeholder="X" step="0.1">`;
                html += `<input type="number" data-path="${fullPath}.y" placeholder="Y" step="0.1">`;
                html += `<input type="number" data-path="${fullPath}.z" placeholder="Z" step="0.1">`;
                html += '</div>';
                break;
            case 'textarea':
                html += `<textarea id="${fullPath}" data-path="${fullPath}" rows="3"></textarea>`;
                break;
            case 'file':
                html += '<div class="file-picker">';
                html += `<input type="text" id="${fullPath}" data-path="${fullPath}">`;
                html += `<button class="btn btn-small" onclick="browseFile('${fullPath}', '${config.fileType || '*'}')">Browse</button>`;
                html += '</div>';
                break;
            default:
                html += `<input type="text" id="${fullPath}" data-path="${fullPath}">`;
        }

        html += '<span class="error-message"></span>';
        html += '</div>';

        return html;
    }
}

// ============================================================================
// Python Editor Integration (CodeMirror)
// ============================================================================

class PythonEditor {
    constructor(containerId, options = {}) {
        this.container = document.getElementById(containerId);
        this.editor = null;
        this.options = Object.assign({
            lineNumbers: true,
            mode: 'python',
            theme: 'material-darker',
            indentUnit: 4,
            tabSize: 4,
            indentWithTabs: false,
            lineWrapping: true,
            autoCloseBrackets: true,
            matchBrackets: true,
            foldGutter: true,
            gutters: ['CodeMirror-linenumbers', 'CodeMirror-foldgutter'],
            extraKeys: {
                'Ctrl-Space': 'autocomplete',
                'Ctrl-/': 'toggleComment',
                'Tab': function(cm) {
                    if (cm.somethingSelected()) {
                        cm.indentSelection('add');
                    } else {
                        cm.replaceSelection('    ', 'end');
                    }
                }
            }
        }, options);

        this.onChange = options.onChange || (() => {});
        this._init();
    }

    _init() {
        if (!this.container || typeof CodeMirror === 'undefined') {
            console.warn('CodeMirror not available or container not found');
            return;
        }

        this.editor = CodeMirror(this.container, this.options);
        this.editor.on('change', () => {
            this.onChange(this.getValue());
        });
    }

    setValue(code) {
        if (this.editor) {
            this.editor.setValue(code || '');
        }
    }

    getValue() {
        return this.editor ? this.editor.getValue() : '';
    }

    setReadOnly(readonly) {
        if (this.editor) {
            this.editor.setOption('readOnly', readonly);
        }
    }

    focus() {
        if (this.editor) {
            this.editor.focus();
        }
    }

    refresh() {
        if (this.editor) {
            this.editor.refresh();
        }
    }
}

// ============================================================================
// WebSocket/Bridge Communication with C++
// ============================================================================

class ViewerBridge {
    constructor() {
        this.connected = false;
        this.messageQueue = [];
        this.eventHandlers = {};
    }

    // Load config from file
    async loadConfig(path) {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.loadConfig', [path], (err, data) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(data);
                }
            });
        });
    }

    // Save config to file
    async saveConfig(path, data) {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.saveConfig', [path, data], (err, result) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(result);
                }
            });
        });
    }

    // Get schema for type
    async getSchema(type) {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.getSchema', [type], (err, schema) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(schema);
                }
            });
        });
    }

    // Browse for file
    async browseFile(filter = '*') {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.browseFile', [filter], (err, path) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(path);
                }
            });
        });
    }

    // Get asset list
    async getAssetList(type) {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.getAssetList', [type], (err, list) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(list);
                }
            });
        });
    }

    // Validate config
    async validateConfig(type, data) {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.validateConfig', [type, data], (err, result) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(result);
                }
            });
        });
    }

    // Execute Python script
    async executePython(script) {
        return new Promise((resolve, reject) => {
            WebEditor.invoke('viewer.executePython', [script], (err, result) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(result);
                }
            });
        });
    }

    // Register event handler
    on(event, handler) {
        if (!this.eventHandlers[event]) {
            this.eventHandlers[event] = [];
        }
        this.eventHandlers[event].push(handler);
    }

    // Remove event handler
    off(event, handler) {
        if (this.eventHandlers[event]) {
            const index = this.eventHandlers[event].indexOf(handler);
            if (index > -1) {
                this.eventHandlers[event].splice(index, 1);
            }
        }
    }

    // Emit event
    emit(event, data) {
        WebEditor.emit(event, data);
    }
}

// ============================================================================
// Targeting Visualizer (for spells/abilities)
// ============================================================================

class TargetingVisualizer {
    constructor(scene) {
        this.scene = scene;
        this.indicator = null;
        this.material = new THREE.MeshBasicMaterial({
            color: 0x00ff00,
            transparent: true,
            opacity: 0.3,
            side: THREE.DoubleSide
        });
    }

    showCircle(radius, position = { x: 0, y: 0.05, z: 0 }) {
        this.clear();
        const geometry = new THREE.CircleGeometry(radius, 32);
        geometry.rotateX(-Math.PI / 2);
        this.indicator = new THREE.Mesh(geometry, this.material.clone());
        this.indicator.position.set(position.x, position.y, position.z);
        this.scene.add(this.indicator);
        return this.indicator;
    }

    showCone(angle, range, position = { x: 0, y: 0.05, z: 0 }) {
        this.clear();
        const geometry = new THREE.CircleGeometry(range, 32, 0, THREE.MathUtils.degToRad(angle));
        geometry.rotateX(-Math.PI / 2);
        this.indicator = new THREE.Mesh(geometry, this.material.clone());
        this.indicator.position.set(position.x, position.y, position.z);
        this.scene.add(this.indicator);
        return this.indicator;
    }

    showLine(width, length, position = { x: 0, y: 0.05, z: 0 }) {
        this.clear();
        const geometry = new THREE.PlaneGeometry(width, length);
        geometry.rotateX(-Math.PI / 2);
        geometry.translate(0, length / 2, 0);
        this.indicator = new THREE.Mesh(geometry, this.material.clone());
        this.indicator.position.set(position.x, position.y, position.z);
        this.scene.add(this.indicator);
        return this.indicator;
    }

    showRectangle(width, height, position = { x: 0, y: 0.05, z: 0 }) {
        this.clear();
        const geometry = new THREE.PlaneGeometry(width, height);
        geometry.rotateX(-Math.PI / 2);
        this.indicator = new THREE.Mesh(geometry, this.material.clone());
        this.indicator.position.set(position.x, position.y, position.z);
        this.scene.add(this.indicator);
        return this.indicator;
    }

    setColor(color) {
        if (this.indicator) {
            this.indicator.material.color.set(color);
        }
    }

    setOpacity(opacity) {
        if (this.indicator) {
            this.indicator.material.opacity = opacity;
        }
    }

    clear() {
        if (this.indicator) {
            this.scene.remove(this.indicator);
            this.indicator.geometry.dispose();
            this.indicator.material.dispose();
            this.indicator = null;
        }
    }
}

// ============================================================================
// Trajectory Visualizer (for projectiles)
// ============================================================================

class TrajectoryVisualizer {
    constructor(scene) {
        this.scene = scene;
        this.line = null;
        this.points = [];
    }

    calculateArc(start, end, height, segments = 50) {
        const points = [];
        for (let i = 0; i <= segments; i++) {
            const t = i / segments;
            const x = start.x + (end.x - start.x) * t;
            const z = start.z + (end.z - start.z) * t;
            const y = start.y + (end.y - start.y) * t +
                      Math.sin(t * Math.PI) * height;
            points.push(new THREE.Vector3(x, y, z));
        }
        return points;
    }

    calculateStraight(start, end, segments = 2) {
        return [
            new THREE.Vector3(start.x, start.y, start.z),
            new THREE.Vector3(end.x, end.y, end.z)
        ];
    }

    calculateHoming(start, end, curveAmount, segments = 50) {
        const points = [];
        const control = new THREE.Vector3(
            (start.x + end.x) / 2 + curveAmount,
            Math.max(start.y, end.y) + curveAmount,
            (start.z + end.z) / 2 + curveAmount
        );

        const curve = new THREE.QuadraticBezierCurve3(
            new THREE.Vector3(start.x, start.y, start.z),
            control,
            new THREE.Vector3(end.x, end.y, end.z)
        );

        return curve.getPoints(segments);
    }

    show(points, color = 0x00ffff) {
        this.clear();

        const geometry = new THREE.BufferGeometry().setFromPoints(points);
        const material = new THREE.LineBasicMaterial({
            color: color,
            linewidth: 2
        });

        this.line = new THREE.Line(geometry, material);
        this.scene.add(this.line);
        this.points = points;

        return this.line;
    }

    clear() {
        if (this.line) {
            this.scene.remove(this.line);
            this.line.geometry.dispose();
            this.line.material.dispose();
            this.line = null;
        }
        this.points = [];
    }

    animate(duration, onComplete) {
        if (!this.line || this.points.length < 2) return;

        const projectile = new THREE.Mesh(
            new THREE.SphereGeometry(0.1, 8, 8),
            new THREE.MeshBasicMaterial({ color: 0xff0000 })
        );
        this.scene.add(projectile);

        let startTime = null;
        const animate = (time) => {
            if (!startTime) startTime = time;
            const elapsed = (time - startTime) / 1000;
            const t = Math.min(elapsed / duration, 1);

            const index = Math.floor(t * (this.points.length - 1));
            const nextIndex = Math.min(index + 1, this.points.length - 1);
            const localT = (t * (this.points.length - 1)) % 1;

            projectile.position.lerpVectors(
                this.points[index],
                this.points[nextIndex],
                localT
            );

            if (t < 1) {
                requestAnimationFrame(animate);
            } else {
                this.scene.remove(projectile);
                if (onComplete) onComplete();
            }
        };

        requestAnimationFrame(animate);
    }
}

// ============================================================================
// Effect Timeline Editor
// ============================================================================

class EffectTimelineEditor {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.effects = [];
        this.duration = 10;
        this.selectedEffect = null;
        this.onUpdate = () => {};

        this._init();
    }

    _init() {
        if (!this.container) return;

        this.container.innerHTML = `
            <div class="timeline-header">
                <span class="timeline-time">0s</span>
                <input type="range" class="timeline-scrubber" min="0" max="100" value="0">
                <span class="timeline-duration">${this.duration}s</span>
            </div>
            <div class="timeline-tracks"></div>
            <div class="timeline-controls">
                <button class="btn btn-small btn-add" onclick="timelineEditor.addEffect()">+ Add Effect</button>
            </div>
        `;

        this.tracksContainer = this.container.querySelector('.timeline-tracks');
        this.scrubber = this.container.querySelector('.timeline-scrubber');
        this.timeDisplay = this.container.querySelector('.timeline-time');

        this.scrubber.addEventListener('input', (e) => {
            const time = (e.target.value / 100) * this.duration;
            this.timeDisplay.textContent = time.toFixed(2) + 's';
        });
    }

    setEffects(effects) {
        this.effects = effects || [];
        this._render();
    }

    getEffects() {
        return this.effects;
    }

    addEffect(effect = {}) {
        const newEffect = Object.assign({
            id: 'effect_' + Date.now(),
            name: 'New Effect',
            startTime: 0,
            duration: 1,
            type: 'damage',
            value: 0
        }, effect);

        this.effects.push(newEffect);
        this._render();
        this.onUpdate(this.effects);
        return newEffect;
    }

    removeEffect(id) {
        const index = this.effects.findIndex(e => e.id === id);
        if (index > -1) {
            this.effects.splice(index, 1);
            this._render();
            this.onUpdate(this.effects);
        }
    }

    _render() {
        if (!this.tracksContainer) return;

        this.tracksContainer.innerHTML = this.effects.map(effect => {
            const left = (effect.startTime / this.duration) * 100;
            const width = (effect.duration / this.duration) * 100;
            return `
                <div class="timeline-track" data-id="${effect.id}">
                    <span class="track-label">${effect.name}</span>
                    <div class="track-bar" style="left: ${left}%; width: ${width}%;"
                         data-id="${effect.id}">
                        <span class="bar-label">${effect.type}</span>
                    </div>
                </div>
            `;
        }).join('');

        // Add click handlers
        this.tracksContainer.querySelectorAll('.track-bar').forEach(bar => {
            bar.addEventListener('click', (e) => {
                const id = e.target.dataset.id;
                this.selectEffect(id);
            });
        });
    }

    selectEffect(id) {
        this.selectedEffect = this.effects.find(e => e.id === id);
        this.container.querySelectorAll('.track-bar').forEach(bar => {
            bar.classList.toggle('selected', bar.dataset.id === id);
        });
    }
}

// ============================================================================
// Node Graph Editor (for tech trees)
// ============================================================================

class NodeGraphEditor {
    constructor(containerId, options = {}) {
        this.container = document.getElementById(containerId);
        this.canvas = null;
        this.ctx = null;
        this.nodes = [];
        this.connections = [];
        this.selectedNode = null;
        this.dragging = null;
        this.connecting = null;
        this.offset = { x: 0, y: 0 };
        this.scale = 1;
        this.pan = { x: 0, y: 0 };
        this.options = Object.assign({
            nodeWidth: 150,
            nodeHeight: 60,
            gridSize: 20,
            snapToGrid: true
        }, options);

        this.onNodeSelect = () => {};
        this.onConnectionCreate = () => {};
        this.onUpdate = () => {};

        this._init();
    }

    _init() {
        if (!this.container) return;

        this.canvas = document.createElement('canvas');
        this.canvas.width = this.container.clientWidth;
        this.canvas.height = this.container.clientHeight;
        this.container.appendChild(this.canvas);
        this.ctx = this.canvas.getContext('2d');

        this._setupEvents();
        this._render();
    }

    _setupEvents() {
        this.canvas.addEventListener('mousedown', (e) => this._onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this._onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this._onMouseUp(e));
        this.canvas.addEventListener('wheel', (e) => this._onWheel(e));
        this.canvas.addEventListener('dblclick', (e) => this._onDoubleClick(e));

        window.addEventListener('resize', () => {
            this.canvas.width = this.container.clientWidth;
            this.canvas.height = this.container.clientHeight;
            this._render();
        });
    }

    _getMousePos(e) {
        const rect = this.canvas.getBoundingClientRect();
        return {
            x: (e.clientX - rect.left - this.pan.x) / this.scale,
            y: (e.clientY - rect.top - this.pan.y) / this.scale
        };
    }

    _getNodeAt(pos) {
        for (let i = this.nodes.length - 1; i >= 0; i--) {
            const node = this.nodes[i];
            if (pos.x >= node.x && pos.x <= node.x + this.options.nodeWidth &&
                pos.y >= node.y && pos.y <= node.y + this.options.nodeHeight) {
                return node;
            }
        }
        return null;
    }

    _onMouseDown(e) {
        const pos = this._getMousePos(e);
        const node = this._getNodeAt(pos);

        if (e.button === 1 || (e.button === 0 && e.altKey)) {
            // Middle click or Alt+click for panning
            this.panning = true;
            this.lastPan = { x: e.clientX, y: e.clientY };
        } else if (node) {
            if (e.shiftKey) {
                // Start connecting
                this.connecting = { from: node, to: pos };
            } else {
                // Start dragging
                this.dragging = node;
                this.offset = {
                    x: pos.x - node.x,
                    y: pos.y - node.y
                };
                this.selectNode(node);
            }
        } else {
            this.deselectNode();
        }
    }

    _onMouseMove(e) {
        const pos = this._getMousePos(e);

        if (this.panning) {
            this.pan.x += e.clientX - this.lastPan.x;
            this.pan.y += e.clientY - this.lastPan.y;
            this.lastPan = { x: e.clientX, y: e.clientY };
            this._render();
        } else if (this.dragging) {
            let newX = pos.x - this.offset.x;
            let newY = pos.y - this.offset.y;

            if (this.options.snapToGrid) {
                newX = Math.round(newX / this.options.gridSize) * this.options.gridSize;
                newY = Math.round(newY / this.options.gridSize) * this.options.gridSize;
            }

            this.dragging.x = newX;
            this.dragging.y = newY;
            this._render();
        } else if (this.connecting) {
            this.connecting.to = pos;
            this._render();
        }
    }

    _onMouseUp(e) {
        if (this.connecting) {
            const pos = this._getMousePos(e);
            const node = this._getNodeAt(pos);
            if (node && node !== this.connecting.from) {
                this.createConnection(this.connecting.from, node);
            }
        }

        this.dragging = null;
        this.connecting = null;
        this.panning = false;
        this.onUpdate(this.getGraphData());
    }

    _onWheel(e) {
        e.preventDefault();
        const delta = e.deltaY > 0 ? 0.9 : 1.1;
        this.scale = Math.max(0.25, Math.min(2, this.scale * delta));
        this._render();
    }

    _onDoubleClick(e) {
        const pos = this._getMousePos(e);
        const node = this._getNodeAt(pos);
        if (!node) {
            this.addNode({
                x: pos.x,
                y: pos.y,
                name: 'New Node',
                data: {}
            });
        }
    }

    _render() {
        if (!this.ctx) return;

        const ctx = this.ctx;
        const canvas = this.canvas;

        // Clear
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Apply transform
        ctx.save();
        ctx.translate(this.pan.x, this.pan.y);
        ctx.scale(this.scale, this.scale);

        // Draw grid
        this._drawGrid();

        // Draw connections
        this._drawConnections();

        // Draw connecting line
        if (this.connecting) {
            ctx.beginPath();
            ctx.strokeStyle = '#007acc';
            ctx.lineWidth = 2;
            const fromX = this.connecting.from.x + this.options.nodeWidth / 2;
            const fromY = this.connecting.from.y + this.options.nodeHeight;
            ctx.moveTo(fromX, fromY);
            ctx.lineTo(this.connecting.to.x, this.connecting.to.y);
            ctx.stroke();
        }

        // Draw nodes
        this._drawNodes();

        ctx.restore();
    }

    _drawGrid() {
        const ctx = this.ctx;
        const gridSize = this.options.gridSize;
        const width = this.canvas.width / this.scale;
        const height = this.canvas.height / this.scale;

        ctx.strokeStyle = '#333';
        ctx.lineWidth = 0.5;

        for (let x = 0; x < width; x += gridSize) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();
        }

        for (let y = 0; y < height; y += gridSize) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
        }
    }

    _drawConnections() {
        const ctx = this.ctx;

        this.connections.forEach(conn => {
            const from = this.nodes.find(n => n.id === conn.from);
            const to = this.nodes.find(n => n.id === conn.to);
            if (!from || !to) return;

            const fromX = from.x + this.options.nodeWidth / 2;
            const fromY = from.y + this.options.nodeHeight;
            const toX = to.x + this.options.nodeWidth / 2;
            const toY = to.y;

            // Draw bezier curve
            ctx.beginPath();
            ctx.strokeStyle = '#666';
            ctx.lineWidth = 2;
            ctx.moveTo(fromX, fromY);
            const cp1y = fromY + 50;
            const cp2y = toY - 50;
            ctx.bezierCurveTo(fromX, cp1y, toX, cp2y, toX, toY);
            ctx.stroke();

            // Draw arrow
            this._drawArrow(toX, toY - 5, 0);
        });
    }

    _drawArrow(x, y, angle) {
        const ctx = this.ctx;
        const size = 8;

        ctx.save();
        ctx.translate(x, y);
        ctx.rotate(angle);
        ctx.beginPath();
        ctx.fillStyle = '#666';
        ctx.moveTo(0, 0);
        ctx.lineTo(-size, -size);
        ctx.lineTo(size, -size);
        ctx.closePath();
        ctx.fill();
        ctx.restore();
    }

    _drawNodes() {
        const ctx = this.ctx;

        this.nodes.forEach(node => {
            const isSelected = this.selectedNode === node;

            // Node background
            ctx.fillStyle = isSelected ? '#094771' : '#2d2d30';
            ctx.strokeStyle = isSelected ? '#007acc' : '#3c3c3c';
            ctx.lineWidth = isSelected ? 2 : 1;

            ctx.beginPath();
            ctx.roundRect(node.x, node.y, this.options.nodeWidth, this.options.nodeHeight, 4);
            ctx.fill();
            ctx.stroke();

            // Node header
            ctx.fillStyle = node.color || '#4a4a50';
            ctx.beginPath();
            ctx.roundRect(node.x, node.y, this.options.nodeWidth, 20, [4, 4, 0, 0]);
            ctx.fill();

            // Node title
            ctx.fillStyle = '#fff';
            ctx.font = '12px sans-serif';
            ctx.textBaseline = 'middle';
            ctx.fillText(
                node.name,
                node.x + 8,
                node.y + 10,
                this.options.nodeWidth - 16
            );

            // Node content
            if (node.subtitle) {
                ctx.fillStyle = '#999';
                ctx.font = '11px sans-serif';
                ctx.fillText(
                    node.subtitle,
                    node.x + 8,
                    node.y + 35,
                    this.options.nodeWidth - 16
                );
            }

            // Connection ports
            ctx.fillStyle = '#007acc';
            ctx.beginPath();
            ctx.arc(node.x + this.options.nodeWidth / 2, node.y, 5, 0, Math.PI * 2);
            ctx.fill();
            ctx.beginPath();
            ctx.arc(node.x + this.options.nodeWidth / 2, node.y + this.options.nodeHeight, 5, 0, Math.PI * 2);
            ctx.fill();
        });
    }

    // Public API

    addNode(config) {
        const node = Object.assign({
            id: 'node_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9),
            x: 100,
            y: 100,
            name: 'Node',
            subtitle: '',
            color: '#4a4a50',
            data: {}
        }, config);

        this.nodes.push(node);
        this._render();
        this.onUpdate(this.getGraphData());
        return node;
    }

    removeNode(id) {
        const index = this.nodes.findIndex(n => n.id === id);
        if (index > -1) {
            this.nodes.splice(index, 1);
            // Remove related connections
            this.connections = this.connections.filter(
                c => c.from !== id && c.to !== id
            );
            this._render();
            this.onUpdate(this.getGraphData());
        }
    }

    selectNode(node) {
        this.selectedNode = node;
        this.onNodeSelect(node);
        this._render();
    }

    deselectNode() {
        this.selectedNode = null;
        this.onNodeSelect(null);
        this._render();
    }

    createConnection(from, to) {
        // Check if connection already exists
        const exists = this.connections.some(
            c => c.from === from.id && c.to === to.id
        );
        if (exists) return null;

        // Check for cycles (simple check)
        if (this._wouldCreateCycle(from.id, to.id)) {
            console.warn('Connection would create a cycle');
            return null;
        }

        const connection = {
            id: 'conn_' + Date.now(),
            from: from.id,
            to: to.id
        };

        this.connections.push(connection);
        this.onConnectionCreate(connection);
        this._render();
        this.onUpdate(this.getGraphData());
        return connection;
    }

    _wouldCreateCycle(fromId, toId) {
        // DFS to check for cycles
        const visited = new Set();
        const stack = [toId];

        while (stack.length > 0) {
            const nodeId = stack.pop();
            if (nodeId === fromId) return true;
            if (visited.has(nodeId)) continue;
            visited.add(nodeId);

            this.connections
                .filter(c => c.from === nodeId)
                .forEach(c => stack.push(c.to));
        }

        return false;
    }

    removeConnection(id) {
        const index = this.connections.findIndex(c => c.id === id);
        if (index > -1) {
            this.connections.splice(index, 1);
            this._render();
            this.onUpdate(this.getGraphData());
        }
    }

    setGraphData(data) {
        this.nodes = data.nodes || [];
        this.connections = data.connections || [];
        this._render();
    }

    getGraphData() {
        return {
            nodes: this.nodes,
            connections: this.connections
        };
    }

    centerView() {
        if (this.nodes.length === 0) return;

        const bounds = this.nodes.reduce((acc, node) => ({
            minX: Math.min(acc.minX, node.x),
            minY: Math.min(acc.minY, node.y),
            maxX: Math.max(acc.maxX, node.x + this.options.nodeWidth),
            maxY: Math.max(acc.maxY, node.y + this.options.nodeHeight)
        }), { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity });

        const centerX = (bounds.minX + bounds.maxX) / 2;
        const centerY = (bounds.minY + bounds.maxY) / 2;

        this.pan.x = this.canvas.width / 2 - centerX * this.scale;
        this.pan.y = this.canvas.height / 2 - centerY * this.scale;
        this._render();
    }

    autoLayout() {
        // Simple hierarchical layout
        const levels = {};
        const visited = new Set();

        // Find root nodes (no incoming connections)
        const roots = this.nodes.filter(node =>
            !this.connections.some(c => c.to === node.id)
        );

        // BFS to assign levels
        const queue = roots.map(n => ({ node: n, level: 0 }));
        while (queue.length > 0) {
            const { node, level } = queue.shift();
            if (visited.has(node.id)) continue;
            visited.add(node.id);

            if (!levels[level]) levels[level] = [];
            levels[level].push(node);

            // Find children
            this.connections
                .filter(c => c.from === node.id)
                .forEach(c => {
                    const child = this.nodes.find(n => n.id === c.to);
                    if (child) queue.push({ node: child, level: level + 1 });
                });
        }

        // Position nodes
        const levelSpacing = 120;
        const nodeSpacing = 180;

        Object.keys(levels).forEach(level => {
            const nodes = levels[level];
            const totalWidth = nodes.length * nodeSpacing;
            const startX = -totalWidth / 2 + nodeSpacing / 2;

            nodes.forEach((node, i) => {
                node.x = startX + i * nodeSpacing + this.canvas.width / 2 / this.scale;
                node.y = parseInt(level) * levelSpacing + 50;
            });
        });

        this._render();
        this.onUpdate(this.getGraphData());
    }
}

// ============================================================================
// Export Global Instance
// ============================================================================

window.ViewerScene = ViewerScene;
window.CollisionShapeUtils = CollisionShapeUtils;
window.SchemaFormGenerator = SchemaFormGenerator;
window.PythonEditor = PythonEditor;
window.ViewerBridge = ViewerBridge;
window.TargetingVisualizer = TargetingVisualizer;
window.TrajectoryVisualizer = TrajectoryVisualizer;
window.EffectTimelineEditor = EffectTimelineEditor;
window.NodeGraphEditor = NodeGraphEditor;

// Create global bridge instance
window.viewerBridge = new ViewerBridge();
