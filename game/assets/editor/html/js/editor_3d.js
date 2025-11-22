/**
 * Nova3D Editor - 3D Preview Utilities
 * Standard Three.js scene setup, camera controls, and gizmos
 * @version 2.0
 */

const Editor3D = (function() {
    'use strict';

    // Check for Three.js
    const hasThree = typeof THREE !== 'undefined';

    // =========================================================================
    // Standard Scene Setup
    // =========================================================================

    class PreviewScene {
        constructor(container, options = {}) {
            if (!hasThree) {
                console.warn('Three.js not loaded. 3D preview unavailable.');
                return;
            }

            this.container = typeof container === 'string' ? document.querySelector(container) : container;
            this.options = {
                antialias: true,
                alpha: false,
                preserveDrawingBuffer: false,
                backgroundColor: 0x1a1a2e,
                showGrid: true,
                showAxes: true,
                gridSize: 20,
                gridDivisions: 20,
                cameraPosition: new THREE.Vector3(5, 5, 5),
                cameraTarget: new THREE.Vector3(0, 0, 0),
                ambientLight: 0x404040,
                directionalLight: 0xffffff,
                ...options
            };

            this._init();
        }

        _init() {
            // Scene
            this.scene = new THREE.Scene();
            this.scene.background = new THREE.Color(this.options.backgroundColor);

            // Camera
            const aspect = this.container.clientWidth / this.container.clientHeight;
            this.camera = new THREE.PerspectiveCamera(45, aspect, 0.1, 1000);
            this.camera.position.copy(this.options.cameraPosition);
            this.camera.lookAt(this.options.cameraTarget);

            // Renderer
            this.renderer = new THREE.WebGLRenderer({
                antialias: this.options.antialias,
                alpha: this.options.alpha,
                preserveDrawingBuffer: this.options.preserveDrawingBuffer
            });
            this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
            this.renderer.setPixelRatio(window.devicePixelRatio);
            this.renderer.shadowMap.enabled = true;
            this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
            this.container.appendChild(this.renderer.domElement);

            // Lighting
            this._setupLighting();

            // Helpers
            if (this.options.showGrid) {
                this._addGrid();
            }
            if (this.options.showAxes) {
                this._addAxes();
            }

            // Orbit Controls
            if (typeof THREE.OrbitControls !== 'undefined') {
                this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
                this.controls.enableDamping = true;
                this.controls.dampingFactor = 0.05;
                this.controls.screenSpacePanning = true;
                this.controls.target.copy(this.options.cameraTarget);
            }

            // Resize handling
            this._resizeObserver = new ResizeObserver(() => this.resize());
            this._resizeObserver.observe(this.container);

            // Animation loop
            this._animate();
        }

        _setupLighting() {
            // Ambient light
            this.ambientLight = new THREE.AmbientLight(this.options.ambientLight);
            this.scene.add(this.ambientLight);

            // Main directional light
            this.directionalLight = new THREE.DirectionalLight(this.options.directionalLight, 0.8);
            this.directionalLight.position.set(10, 20, 10);
            this.directionalLight.castShadow = true;
            this.directionalLight.shadow.mapSize.width = 2048;
            this.directionalLight.shadow.mapSize.height = 2048;
            this.directionalLight.shadow.camera.near = 0.5;
            this.directionalLight.shadow.camera.far = 50;
            this.scene.add(this.directionalLight);

            // Fill light
            this.fillLight = new THREE.DirectionalLight(0xffffff, 0.3);
            this.fillLight.position.set(-10, 10, -10);
            this.scene.add(this.fillLight);

            // Hemisphere light for ambient variation
            this.hemisphereLight = new THREE.HemisphereLight(0x87ceeb, 0x6c5240, 0.3);
            this.scene.add(this.hemisphereLight);
        }

        _addGrid() {
            this.grid = new THREE.GridHelper(
                this.options.gridSize,
                this.options.gridDivisions,
                0x444444,
                0x222222
            );
            this.scene.add(this.grid);
        }

        _addAxes() {
            this.axes = new THREE.AxesHelper(5);
            this.scene.add(this.axes);
        }

        _animate() {
            this._animationId = requestAnimationFrame(() => this._animate());

            if (this.controls) {
                this.controls.update();
            }

            this.renderer.render(this.scene, this.camera);

            if (this.onUpdate) {
                this.onUpdate();
            }
        }

        resize() {
            const width = this.container.clientWidth;
            const height = this.container.clientHeight;

            this.camera.aspect = width / height;
            this.camera.updateProjectionMatrix();
            this.renderer.setSize(width, height);
        }

        /**
         * Add an object to the scene
         * @param {THREE.Object3D} object
         */
        add(object) {
            this.scene.add(object);
        }

        /**
         * Remove an object from the scene
         * @param {THREE.Object3D} object
         */
        remove(object) {
            this.scene.remove(object);
        }

        /**
         * Clear all user objects (keep lights and helpers)
         */
        clearObjects() {
            const toRemove = [];
            this.scene.traverse(obj => {
                if (obj !== this.scene &&
                    obj !== this.ambientLight &&
                    obj !== this.directionalLight &&
                    obj !== this.fillLight &&
                    obj !== this.hemisphereLight &&
                    obj !== this.grid &&
                    obj !== this.axes &&
                    !obj.isLight) {
                    toRemove.push(obj);
                }
            });
            toRemove.forEach(obj => {
                this.scene.remove(obj);
                if (obj.geometry) obj.geometry.dispose();
                if (obj.material) {
                    if (Array.isArray(obj.material)) {
                        obj.material.forEach(m => m.dispose());
                    } else {
                        obj.material.dispose();
                    }
                }
            });
        }

        /**
         * Focus camera on an object
         * @param {THREE.Object3D} object
         * @param {number} padding
         */
        focusOn(object, padding = 1.5) {
            const box = new THREE.Box3().setFromObject(object);
            const center = box.getCenter(new THREE.Vector3());
            const size = box.getSize(new THREE.Vector3());

            const maxDim = Math.max(size.x, size.y, size.z);
            const fov = this.camera.fov * (Math.PI / 180);
            let distance = maxDim / (2 * Math.tan(fov / 2)) * padding;

            this.camera.position.copy(center).add(new THREE.Vector3(distance, distance * 0.5, distance));
            if (this.controls) {
                this.controls.target.copy(center);
                this.controls.update();
            }
        }

        /**
         * Set camera view
         * @param {string} view - 'front', 'back', 'left', 'right', 'top', 'bottom', 'perspective'
         */
        setView(view) {
            const distance = 10;
            const target = this.controls ? this.controls.target : new THREE.Vector3(0, 0, 0);

            const positions = {
                front: new THREE.Vector3(0, 0, distance),
                back: new THREE.Vector3(0, 0, -distance),
                left: new THREE.Vector3(-distance, 0, 0),
                right: new THREE.Vector3(distance, 0, 0),
                top: new THREE.Vector3(0, distance, 0),
                bottom: new THREE.Vector3(0, -distance, 0),
                perspective: new THREE.Vector3(distance, distance * 0.5, distance)
            };

            if (positions[view]) {
                this.camera.position.copy(target).add(positions[view]);
                this.camera.lookAt(target);
                if (this.controls) {
                    this.controls.update();
                }
            }
        }

        /**
         * Toggle grid visibility
         */
        toggleGrid() {
            if (this.grid) {
                this.grid.visible = !this.grid.visible;
            }
        }

        /**
         * Toggle axes visibility
         */
        toggleAxes() {
            if (this.axes) {
                this.axes.visible = !this.axes.visible;
            }
        }

        /**
         * Set background color
         * @param {number|string} color
         */
        setBackgroundColor(color) {
            this.scene.background = new THREE.Color(color);
        }

        /**
         * Take a screenshot
         * @returns {string} Data URL
         */
        screenshot() {
            this.renderer.render(this.scene, this.camera);
            return this.renderer.domElement.toDataURL('image/png');
        }

        /**
         * Dispose of the scene
         */
        dispose() {
            cancelAnimationFrame(this._animationId);
            this._resizeObserver.disconnect();

            if (this.controls) {
                this.controls.dispose();
            }

            this.scene.traverse(obj => {
                if (obj.geometry) obj.geometry.dispose();
                if (obj.material) {
                    if (Array.isArray(obj.material)) {
                        obj.material.forEach(m => m.dispose());
                    } else {
                        obj.material.dispose();
                    }
                }
            });

            this.renderer.dispose();
            this.container.removeChild(this.renderer.domElement);
        }
    }

    // =========================================================================
    // Selection Highlighting
    // =========================================================================

    class SelectionManager {
        constructor(scene) {
            this.scene = scene;
            this.selected = new Set();
            this.outlinePass = null;
            this.selectionBox = null;
        }

        /**
         * Select an object
         * @param {THREE.Object3D} object
         * @param {boolean} addToSelection
         */
        select(object, addToSelection = false) {
            if (!addToSelection) {
                this.deselectAll();
            }

            this.selected.add(object);
            this._updateHighlight(object, true);

            EditorCore?.Events?.emit('selection:change', {
                selected: Array.from(this.selected)
            });
        }

        /**
         * Deselect an object
         * @param {THREE.Object3D} object
         */
        deselect(object) {
            this.selected.delete(object);
            this._updateHighlight(object, false);

            EditorCore?.Events?.emit('selection:change', {
                selected: Array.from(this.selected)
            });
        }

        /**
         * Deselect all objects
         */
        deselectAll() {
            this.selected.forEach(obj => {
                this._updateHighlight(obj, false);
            });
            this.selected.clear();

            EditorCore?.Events?.emit('selection:change', { selected: [] });
        }

        /**
         * Toggle selection
         * @param {THREE.Object3D} object
         */
        toggle(object) {
            if (this.selected.has(object)) {
                this.deselect(object);
            } else {
                this.select(object, true);
            }
        }

        /**
         * Check if an object is selected
         * @param {THREE.Object3D} object
         * @returns {boolean}
         */
        isSelected(object) {
            return this.selected.has(object);
        }

        _updateHighlight(object, highlighted) {
            if (highlighted) {
                // Add outline effect
                object.userData.originalMaterials = object.userData.originalMaterials || [];

                object.traverse(child => {
                    if (child.isMesh) {
                        child.userData.originalMaterial = child.material;
                        // Create highlighted material
                        const highlightMaterial = child.material.clone();
                        highlightMaterial.emissive = new THREE.Color(0x3b82f6);
                        highlightMaterial.emissiveIntensity = 0.3;
                        child.material = highlightMaterial;
                    }
                });

                // Add selection box
                if (!this.selectionBox) {
                    this.selectionBox = new THREE.BoxHelper(object, 0x3b82f6);
                    this.scene.add(this.selectionBox);
                } else {
                    this.selectionBox.setFromObject(object);
                }
            } else {
                // Remove highlight
                object.traverse(child => {
                    if (child.isMesh && child.userData.originalMaterial) {
                        child.material.dispose();
                        child.material = child.userData.originalMaterial;
                        delete child.userData.originalMaterial;
                    }
                });

                if (this.selectionBox && this.selected.size === 0) {
                    this.scene.remove(this.selectionBox);
                    this.selectionBox = null;
                }
            }
        }
    }

    // =========================================================================
    // Transform Gizmo
    // =========================================================================

    class TransformGizmo {
        constructor(scene, camera, renderer) {
            if (!hasThree || typeof THREE.TransformControls === 'undefined') {
                console.warn('TransformControls not available');
                return;
            }

            this.scene = scene;
            this.camera = camera;
            this.renderer = renderer;

            this.controls = new THREE.TransformControls(camera, renderer.domElement);
            scene.add(this.controls);

            this._currentMode = 'translate';
            this._currentSpace = 'world';

            this.controls.addEventListener('dragging-changed', (event) => {
                EditorCore?.Events?.emit('gizmo:dragging', { dragging: event.value });
            });

            this.controls.addEventListener('change', () => {
                EditorCore?.Events?.emit('gizmo:change', {
                    object: this.controls.object,
                    mode: this._currentMode
                });
            });
        }

        /**
         * Attach gizmo to an object
         * @param {THREE.Object3D} object
         */
        attach(object) {
            if (this.controls) {
                this.controls.attach(object);
            }
        }

        /**
         * Detach gizmo
         */
        detach() {
            if (this.controls) {
                this.controls.detach();
            }
        }

        /**
         * Set transform mode
         * @param {string} mode - 'translate', 'rotate', 'scale'
         */
        setMode(mode) {
            if (this.controls) {
                this._currentMode = mode;
                this.controls.setMode(mode);
            }
        }

        /**
         * Set transform space
         * @param {string} space - 'world' or 'local'
         */
        setSpace(space) {
            if (this.controls) {
                this._currentSpace = space;
                this.controls.setSpace(space);
            }
        }

        /**
         * Toggle snapping
         * @param {boolean} enabled
         * @param {number} value
         */
        setSnap(enabled, value = 1) {
            if (this.controls) {
                this.controls.setTranslationSnap(enabled ? value : null);
                this.controls.setRotationSnap(enabled ? THREE.MathUtils.degToRad(15) : null);
                this.controls.setScaleSnap(enabled ? 0.1 : null);
            }
        }

        dispose() {
            if (this.controls) {
                this.scene.remove(this.controls);
                this.controls.dispose();
            }
        }
    }

    // =========================================================================
    // Raycaster / Picking
    // =========================================================================

    class ObjectPicker {
        constructor(camera, scene, container) {
            this.camera = camera;
            this.scene = scene;
            this.container = container;
            this.raycaster = hasThree ? new THREE.Raycaster() : null;
            this.mouse = hasThree ? new THREE.Vector2() : null;
            this.excludeList = [];
        }

        /**
         * Pick objects at mouse position
         * @param {MouseEvent} event
         * @param {boolean} recursive
         * @returns {Array}
         */
        pick(event, recursive = true) {
            if (!this.raycaster) return [];

            const rect = this.container.getBoundingClientRect();
            this.mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
            this.mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

            this.raycaster.setFromCamera(this.mouse, this.camera);

            const objects = [];
            this.scene.traverse(obj => {
                if (obj.isMesh && !this.excludeList.includes(obj)) {
                    objects.push(obj);
                }
            });

            return this.raycaster.intersectObjects(objects, recursive);
        }

        /**
         * Pick the first object
         * @param {MouseEvent} event
         * @returns {Object|null}
         */
        pickFirst(event) {
            const intersects = this.pick(event);
            return intersects.length > 0 ? intersects[0] : null;
        }

        /**
         * Exclude objects from picking
         * @param {Array} objects
         */
        exclude(objects) {
            this.excludeList = objects;
        }
    }

    // =========================================================================
    // Model Loader Helper
    // =========================================================================

    const ModelLoader = {
        /**
         * Load a GLTF/GLB model
         * @param {string} url
         * @returns {Promise}
         */
        loadGLTF(url) {
            return new Promise((resolve, reject) => {
                if (typeof THREE.GLTFLoader === 'undefined') {
                    reject(new Error('GLTFLoader not available'));
                    return;
                }

                const loader = new THREE.GLTFLoader();
                loader.load(
                    url,
                    (gltf) => resolve(gltf),
                    (progress) => EditorCore?.Events?.emit('model:loading', {
                        loaded: progress.loaded,
                        total: progress.total
                    }),
                    (error) => reject(error)
                );
            });
        },

        /**
         * Load an OBJ model
         * @param {string} url
         * @param {string} mtlUrl
         * @returns {Promise}
         */
        loadOBJ(url, mtlUrl = null) {
            return new Promise((resolve, reject) => {
                if (typeof THREE.OBJLoader === 'undefined') {
                    reject(new Error('OBJLoader not available'));
                    return;
                }

                const objLoader = new THREE.OBJLoader();

                if (mtlUrl && typeof THREE.MTLLoader !== 'undefined') {
                    const mtlLoader = new THREE.MTLLoader();
                    mtlLoader.load(mtlUrl, (materials) => {
                        materials.preload();
                        objLoader.setMaterials(materials);
                        objLoader.load(url, resolve, undefined, reject);
                    });
                } else {
                    objLoader.load(url, resolve, undefined, reject);
                }
            });
        },

        /**
         * Create a simple placeholder mesh
         * @param {string} type - 'box', 'sphere', 'cylinder', 'plane'
         * @param {Object} options
         * @returns {THREE.Mesh}
         */
        createPrimitive(type, options = {}) {
            if (!hasThree) return null;

            const color = options.color || 0x3b82f6;
            const material = new THREE.MeshStandardMaterial({
                color,
                metalness: 0.3,
                roughness: 0.7
            });

            let geometry;
            switch (type) {
                case 'box':
                    geometry = new THREE.BoxGeometry(
                        options.width || 1,
                        options.height || 1,
                        options.depth || 1
                    );
                    break;
                case 'sphere':
                    geometry = new THREE.SphereGeometry(
                        options.radius || 0.5,
                        options.segments || 32,
                        options.segments || 32
                    );
                    break;
                case 'cylinder':
                    geometry = new THREE.CylinderGeometry(
                        options.radiusTop || 0.5,
                        options.radiusBottom || 0.5,
                        options.height || 1,
                        options.segments || 32
                    );
                    break;
                case 'plane':
                    geometry = new THREE.PlaneGeometry(
                        options.width || 1,
                        options.height || 1
                    );
                    break;
                default:
                    geometry = new THREE.BoxGeometry(1, 1, 1);
            }

            const mesh = new THREE.Mesh(geometry, material);
            mesh.castShadow = true;
            mesh.receiveShadow = true;

            return mesh;
        }
    };

    // =========================================================================
    // Public API
    // =========================================================================

    return {
        PreviewScene,
        SelectionManager,
        TransformGizmo,
        ObjectPicker,
        ModelLoader,
        hasThree
    };
})();

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = Editor3D;
}

if (typeof window !== 'undefined') {
    window.Editor3D = Editor3D;
}
