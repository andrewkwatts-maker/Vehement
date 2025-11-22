/**
 * Menu Core - RTS Game Menu System Utilities
 * Provides page transitions, animations, sound effects, and backend communication
 */

(function(global) {
    'use strict';

    // ===== Menu State Machine =====
    const MenuState = {
        MAIN: 'main',
        CAMPAIGN: 'campaign',
        MULTIPLAYER: 'multiplayer',
        CUSTOM_GAMES: 'custom_games',
        EDITOR: 'editor',
        SETTINGS: 'settings',
        LOADING: 'loading',
        CREDITS: 'credits'
    };

    // ===== Menu Navigation System =====
    const MenuNavigation = {
        _history: [],
        _currentState: null,
        _transitioning: false,
        _transitionDuration: 300,

        /**
         * Initialize navigation system
         */
        init: function() {
            this._currentState = MenuState.MAIN;
            this._history = [];

            // Handle browser back button
            window.addEventListener('popstate', (e) => {
                if (e.state && e.state.menu) {
                    this._navigateWithoutHistory(e.state.menu);
                }
            });

            // Keyboard navigation
            document.addEventListener('keydown', (e) => {
                if (e.key === 'Escape' && !this._transitioning) {
                    this.back();
                }
            });
        },

        /**
         * Navigate to a menu state
         */
        navigateTo: function(state, data = {}) {
            if (this._transitioning || state === this._currentState) return;

            const prevState = this._currentState;
            this._history.push({ state: prevState, data: {} });
            this._currentState = state;

            // Update browser history
            history.pushState({ menu: state }, '', `#${state}`);

            this._performTransition(prevState, state, data);

            // Notify engine
            Engine.call('Menu.onNavigate', {
                from: prevState,
                to: state,
                data: data
            });
        },

        /**
         * Go back to previous menu
         */
        back: function() {
            if (this._transitioning || this._history.length === 0) return;

            const prev = this._history.pop();
            const currentState = this._currentState;
            this._currentState = prev.state;

            history.back();

            this._performTransition(currentState, prev.state, prev.data, true);

            Engine.call('Menu.onNavigateBack', {
                from: currentState,
                to: prev.state
            });
        },

        /**
         * Internal navigation without history
         */
        _navigateWithoutHistory: function(state) {
            if (this._transitioning || state === this._currentState) return;

            const prevState = this._currentState;
            this._currentState = state;
            this._performTransition(prevState, state, {});
        },

        /**
         * Perform page transition animation
         */
        _performTransition: function(from, to, data, isBack = false) {
            this._transitioning = true;

            const fromElement = document.getElementById(`menu-${from}`);
            const toElement = document.getElementById(`menu-${to}`);

            if (!toElement) {
                console.error(`Menu element not found: menu-${to}`);
                this._transitioning = false;
                return;
            }

            // Choose transition type
            const transitionType = isBack ? 'slide-right' : 'slide-left';

            // Exit animation for current page
            if (fromElement) {
                MenuAnimations.exitPage(fromElement, transitionType, () => {
                    fromElement.classList.add('menu-hidden');
                });
            }

            // Enter animation for new page
            toElement.classList.remove('menu-hidden');
            MenuAnimations.enterPage(toElement, transitionType, () => {
                this._transitioning = false;

                // Trigger page-specific initialization
                if (global.MenuPages && global.MenuPages[to]) {
                    global.MenuPages[to].onEnter(data);
                }
            });

            // Play transition sound
            MenuAudio.playSound('menu_transition');
        },

        /**
         * Get current state
         */
        getCurrentState: function() {
            return this._currentState;
        },

        /**
         * Check if can go back
         */
        canGoBack: function() {
            return this._history.length > 0;
        },

        /**
         * Reset to main menu
         */
        reset: function() {
            this._history = [];
            this._currentState = MenuState.MAIN;
            this._transitioning = false;
        }
    };

    // ===== Menu Animations =====
    const MenuAnimations = {
        _defaultDuration: 300,

        /**
         * Enter page animation
         */
        enterPage: function(element, type = 'fade', callback) {
            const duration = this._defaultDuration;

            switch (type) {
                case 'fade':
                    this._animate(element, {
                        opacity: [0, 1]
                    }, duration, callback);
                    break;

                case 'slide-left':
                    this._animate(element, {
                        opacity: [0, 1],
                        transform: ['translateX(50px)', 'translateX(0)']
                    }, duration, callback);
                    break;

                case 'slide-right':
                    this._animate(element, {
                        opacity: [0, 1],
                        transform: ['translateX(-50px)', 'translateX(0)']
                    }, duration, callback);
                    break;

                case 'slide-up':
                    this._animate(element, {
                        opacity: [0, 1],
                        transform: ['translateY(30px)', 'translateY(0)']
                    }, duration, callback);
                    break;

                case 'scale':
                    this._animate(element, {
                        opacity: [0, 1],
                        transform: ['scale(0.95)', 'scale(1)']
                    }, duration, callback);
                    break;

                default:
                    if (callback) callback();
            }
        },

        /**
         * Exit page animation
         */
        exitPage: function(element, type = 'fade', callback) {
            const duration = this._defaultDuration;

            switch (type) {
                case 'fade':
                    this._animate(element, {
                        opacity: [1, 0]
                    }, duration, callback);
                    break;

                case 'slide-left':
                    this._animate(element, {
                        opacity: [1, 0],
                        transform: ['translateX(0)', 'translateX(-50px)']
                    }, duration, callback);
                    break;

                case 'slide-right':
                    this._animate(element, {
                        opacity: [1, 0],
                        transform: ['translateX(0)', 'translateX(50px)']
                    }, duration, callback);
                    break;

                case 'slide-up':
                    this._animate(element, {
                        opacity: [1, 0],
                        transform: ['translateY(0)', 'translateY(-30px)']
                    }, duration, callback);
                    break;

                case 'scale':
                    this._animate(element, {
                        opacity: [1, 0],
                        transform: ['scale(1)', 'scale(0.95)']
                    }, duration, callback);
                    break;

                default:
                    if (callback) callback();
            }
        },

        /**
         * Stagger animation for list items
         */
        staggerIn: function(container, selector = '> *', delay = 50) {
            const items = container.querySelectorAll(selector);
            items.forEach((item, index) => {
                item.style.opacity = '0';
                item.style.transform = 'translateY(20px)';

                setTimeout(() => {
                    this._animate(item, {
                        opacity: [0, 1],
                        transform: ['translateY(20px)', 'translateY(0)']
                    }, 300);
                }, index * delay);
            });
        },

        /**
         * Button hover effect
         */
        buttonHover: function(element, entering) {
            if (entering) {
                element.style.transform = 'scale(1.02)';
            } else {
                element.style.transform = 'scale(1)';
            }
        },

        /**
         * Button click effect
         */
        buttonClick: function(element) {
            element.style.transform = 'scale(0.98)';
            setTimeout(() => {
                element.style.transform = 'scale(1)';
            }, 100);
        },

        /**
         * Pulse animation
         */
        pulse: function(element, duration = 2000) {
            element.style.animation = `menu-pulse ${duration}ms ease-in-out infinite`;
        },

        /**
         * Stop pulse animation
         */
        stopPulse: function(element) {
            element.style.animation = '';
        },

        /**
         * Glow animation
         */
        glow: function(element, color = 'rgba(212, 168, 75, 0.5)') {
            element.style.boxShadow = `0 0 20px ${color}`;
        },

        /**
         * Shake animation (for errors)
         */
        shake: function(element) {
            const keyframes = [
                { transform: 'translateX(0)' },
                { transform: 'translateX(-10px)' },
                { transform: 'translateX(10px)' },
                { transform: 'translateX(-10px)' },
                { transform: 'translateX(10px)' },
                { transform: 'translateX(0)' }
            ];

            element.animate(keyframes, {
                duration: 400,
                easing: 'ease-in-out'
            });
        },

        /**
         * Internal animation helper
         */
        _animate: function(element, properties, duration, callback) {
            const keyframes = {};
            const from = {};
            const to = {};

            Object.entries(properties).forEach(([prop, values]) => {
                from[prop] = values[0];
                to[prop] = values[1];
            });

            const animation = element.animate([from, to], {
                duration: duration,
                easing: 'ease-out',
                fill: 'forwards'
            });

            if (callback) {
                animation.onfinish = callback;
            }

            return animation;
        }
    };

    // ===== Parallax Background System =====
    const ParallaxBackground = {
        _layers: [],
        _mouseEnabled: true,
        _mouseX: 0,
        _mouseY: 0,
        _targetX: 0,
        _targetY: 0,

        /**
         * Initialize parallax system
         */
        init: function(container) {
            this._container = container;
            this._layers = Array.from(container.querySelectorAll('.parallax-layer'));

            if (this._mouseEnabled) {
                document.addEventListener('mousemove', (e) => {
                    this._mouseX = (e.clientX / window.innerWidth - 0.5) * 2;
                    this._mouseY = (e.clientY / window.innerHeight - 0.5) * 2;
                });

                this._animate();
            }
        },

        /**
         * Set layer configuration
         */
        setLayers: function(config) {
            config.forEach((layerConfig, index) => {
                if (this._layers[index]) {
                    const layer = this._layers[index];
                    if (layerConfig.image) {
                        layer.style.backgroundImage = `url('${layerConfig.image}')`;
                    }
                    layer.dataset.depth = layerConfig.depth || (index + 1) * 0.1;
                }
            });
        },

        /**
         * Animation loop
         */
        _animate: function() {
            // Smooth interpolation
            this._targetX += (this._mouseX - this._targetX) * 0.05;
            this._targetY += (this._mouseY - this._targetY) * 0.05;

            this._layers.forEach((layer) => {
                const depth = parseFloat(layer.dataset.depth) || 0.1;
                const moveX = this._targetX * depth * 20;
                const moveY = this._targetY * depth * 20;

                layer.style.transform = `translate(${moveX}px, ${moveY}px)`;
            });

            requestAnimationFrame(() => this._animate());
        },

        /**
         * Enable/disable mouse parallax
         */
        setMouseEnabled: function(enabled) {
            this._mouseEnabled = enabled;
        }
    };

    // ===== Audio Manager =====
    const MenuAudio = {
        _sounds: {},
        _musicVolume: 0.7,
        _sfxVolume: 0.8,
        _currentMusic: null,
        _musicElement: null,
        _muted: false,

        /**
         * Initialize audio system
         */
        init: function() {
            this._musicElement = document.createElement('audio');
            this._musicElement.loop = true;
            document.body.appendChild(this._musicElement);

            // Load volume settings
            const savedMusic = localStorage.getItem('menu_music_volume');
            const savedSfx = localStorage.getItem('menu_sfx_volume');

            if (savedMusic !== null) this._musicVolume = parseFloat(savedMusic);
            if (savedSfx !== null) this._sfxVolume = parseFloat(savedSfx);

            this._updateMusicVolume();
        },

        /**
         * Register a sound effect
         */
        registerSound: function(name, url) {
            const audio = new Audio(url);
            audio.preload = 'auto';
            this._sounds[name] = audio;
        },

        /**
         * Play sound effect
         */
        playSound: function(name) {
            if (this._muted) return;

            const sound = this._sounds[name];
            if (sound) {
                const clone = sound.cloneNode();
                clone.volume = this._sfxVolume;
                clone.play().catch(() => {});
            } else {
                // Notify engine to play sound
                Engine.call('Audio.playSound', { name: name, volume: this._sfxVolume });
            }
        },

        /**
         * Play background music
         */
        playMusic: function(url, fadeIn = true) {
            if (this._currentMusic === url) return;

            this._currentMusic = url;

            if (fadeIn && this._musicElement.src) {
                this._fadeOut(() => {
                    this._musicElement.src = url;
                    this._musicElement.play().catch(() => {});
                    this._fadeIn();
                });
            } else {
                this._musicElement.src = url;
                this._musicElement.volume = this._muted ? 0 : this._musicVolume;
                this._musicElement.play().catch(() => {});
            }
        },

        /**
         * Stop music
         */
        stopMusic: function(fadeOut = true) {
            if (fadeOut) {
                this._fadeOut(() => {
                    this._musicElement.pause();
                    this._musicElement.currentTime = 0;
                });
            } else {
                this._musicElement.pause();
                this._musicElement.currentTime = 0;
            }
            this._currentMusic = null;
        },

        /**
         * Pause music
         */
        pauseMusic: function() {
            this._musicElement.pause();
        },

        /**
         * Resume music
         */
        resumeMusic: function() {
            this._musicElement.play().catch(() => {});
        },

        /**
         * Set music volume
         */
        setMusicVolume: function(volume) {
            this._musicVolume = Math.max(0, Math.min(1, volume));
            localStorage.setItem('menu_music_volume', this._musicVolume);
            this._updateMusicVolume();
        },

        /**
         * Set SFX volume
         */
        setSfxVolume: function(volume) {
            this._sfxVolume = Math.max(0, Math.min(1, volume));
            localStorage.setItem('menu_sfx_volume', this._sfxVolume);
        },

        /**
         * Get music volume
         */
        getMusicVolume: function() {
            return this._musicVolume;
        },

        /**
         * Get SFX volume
         */
        getSfxVolume: function() {
            return this._sfxVolume;
        },

        /**
         * Toggle mute
         */
        toggleMute: function() {
            this._muted = !this._muted;
            this._updateMusicVolume();
            return this._muted;
        },

        /**
         * Check if muted
         */
        isMuted: function() {
            return this._muted;
        },

        _updateMusicVolume: function() {
            this._musicElement.volume = this._muted ? 0 : this._musicVolume;
        },

        _fadeIn: function(duration = 1000) {
            this._musicElement.volume = 0;
            const targetVolume = this._muted ? 0 : this._musicVolume;
            const step = targetVolume / (duration / 50);

            const fade = setInterval(() => {
                if (this._musicElement.volume < targetVolume - step) {
                    this._musicElement.volume += step;
                } else {
                    this._musicElement.volume = targetVolume;
                    clearInterval(fade);
                }
            }, 50);
        },

        _fadeOut: function(callback, duration = 500) {
            const step = this._musicElement.volume / (duration / 50);

            const fade = setInterval(() => {
                if (this._musicElement.volume > step) {
                    this._musicElement.volume -= step;
                } else {
                    this._musicElement.volume = 0;
                    clearInterval(fade);
                    if (callback) callback();
                }
            }, 50);
        }
    };

    // ===== Backend Communication =====
    const MenuBackend = {
        _requestQueue: [],
        _processing: false,

        /**
         * Send request to game backend
         */
        request: function(endpoint, data = {}) {
            return new Promise((resolve, reject) => {
                Engine.call(`Menu.${endpoint}`, data, (result, error) => {
                    if (error) {
                        reject(error);
                    } else {
                        resolve(result);
                    }
                });
            });
        },

        /**
         * Get player profile data
         */
        getPlayerProfile: function() {
            return this.request('getPlayerProfile');
        },

        /**
         * Get campaign progress
         */
        getCampaignProgress: function() {
            return this.request('getCampaignProgress');
        },

        /**
         * Get multiplayer server list
         */
        getServerList: function(filters = {}) {
            return this.request('getServerList', filters);
        },

        /**
         * Get custom games list
         */
        getCustomGames: function(filters = {}) {
            return this.request('getCustomGames', filters);
        },

        /**
         * Get news/updates
         */
        getNews: function() {
            return this.request('getNews');
        },

        /**
         * Get settings
         */
        getSettings: function() {
            return this.request('getSettings');
        },

        /**
         * Save settings
         */
        saveSettings: function(settings) {
            return this.request('saveSettings', settings);
        },

        /**
         * Start campaign mission
         */
        startCampaignMission: function(raceId, chapterId, difficulty) {
            return this.request('startCampaignMission', {
                raceId, chapterId, difficulty
            });
        },

        /**
         * Join multiplayer game
         */
        joinMultiplayerGame: function(gameId) {
            return this.request('joinMultiplayerGame', { gameId });
        },

        /**
         * Create multiplayer lobby
         */
        createLobby: function(settings) {
            return this.request('createLobby', settings);
        },

        /**
         * Join lobby with code
         */
        joinLobbyWithCode: function(code) {
            return this.request('joinLobbyWithCode', { code });
        },

        /**
         * Host custom game
         */
        hostCustomGame: function(settings) {
            return this.request('hostCustomGame', settings);
        },

        /**
         * Exit game
         */
        exitGame: function() {
            return this.request('exitGame');
        }
    };

    // ===== UI Components =====
    const MenuComponents = {
        /**
         * Create a menu button
         */
        createButton: function(options) {
            const btn = document.createElement('button');
            btn.className = 'menu-btn';

            if (options.primary) btn.classList.add('menu-btn-primary');
            if (options.danger) btn.classList.add('menu-btn-danger');
            if (options.small) btn.classList.add('menu-btn-sm');
            if (options.large) btn.classList.add('menu-btn-lg');
            if (options.className) btn.classList.add(options.className);

            if (options.icon) {
                btn.classList.add('menu-btn-icon');
                btn.innerHTML = `<img src="${options.icon}" alt=""><span>${options.text}</span>`;
            } else {
                btn.textContent = options.text;
            }

            if (options.disabled) btn.disabled = true;

            // Event handlers
            btn.addEventListener('mouseenter', () => {
                MenuAudio.playSound('button_hover');
                MenuAnimations.buttonHover(btn, true);
            });

            btn.addEventListener('mouseleave', () => {
                MenuAnimations.buttonHover(btn, false);
            });

            btn.addEventListener('click', (e) => {
                MenuAudio.playSound('button_click');
                MenuAnimations.buttonClick(btn);
                if (options.onClick) options.onClick(e);
            });

            return btn;
        },

        /**
         * Create a tab container
         */
        createTabs: function(tabs, container) {
            const tabsNav = document.createElement('div');
            tabsNav.className = 'menu-tabs';

            const tabsContent = document.createElement('div');
            tabsContent.className = 'menu-tabs-content';

            tabs.forEach((tab, index) => {
                // Create tab button
                const tabBtn = document.createElement('button');
                tabBtn.className = 'menu-tab' + (index === 0 ? ' active' : '');
                tabBtn.textContent = tab.label;
                tabBtn.dataset.tab = tab.id;

                tabBtn.addEventListener('click', () => {
                    // Update active states
                    tabsNav.querySelectorAll('.menu-tab').forEach(t => t.classList.remove('active'));
                    tabsContent.querySelectorAll('.menu-tab-content').forEach(c => c.classList.remove('active'));

                    tabBtn.classList.add('active');
                    const content = tabsContent.querySelector(`[data-tab-content="${tab.id}"]`);
                    if (content) content.classList.add('active');

                    MenuAudio.playSound('tab_switch');
                });

                tabsNav.appendChild(tabBtn);

                // Create tab content
                const tabContent = document.createElement('div');
                tabContent.className = 'menu-tab-content' + (index === 0 ? ' active' : '');
                tabContent.dataset.tabContent = tab.id;
                if (tab.content) {
                    if (typeof tab.content === 'string') {
                        tabContent.innerHTML = tab.content;
                    } else {
                        tabContent.appendChild(tab.content);
                    }
                }
                tabsContent.appendChild(tabContent);
            });

            container.appendChild(tabsNav);
            container.appendChild(tabsContent);

            return { nav: tabsNav, content: tabsContent };
        },

        /**
         * Create a slider component
         */
        createSlider: function(options) {
            const container = document.createElement('div');
            container.className = 'menu-slider-container';

            const slider = document.createElement('input');
            slider.type = 'range';
            slider.className = 'menu-slider';
            slider.min = options.min || 0;
            slider.max = options.max || 100;
            slider.step = options.step || 1;
            slider.value = options.value || 50;

            const valueDisplay = document.createElement('span');
            valueDisplay.className = 'menu-slider-value';
            valueDisplay.textContent = options.formatValue ?
                options.formatValue(slider.value) : slider.value;

            slider.addEventListener('input', () => {
                valueDisplay.textContent = options.formatValue ?
                    options.formatValue(slider.value) : slider.value;
                if (options.onChange) options.onChange(parseFloat(slider.value));
            });

            container.appendChild(slider);
            container.appendChild(valueDisplay);

            return { container, slider, valueDisplay };
        },

        /**
         * Create a checkbox component
         */
        createCheckbox: function(options) {
            const label = document.createElement('label');
            label.className = 'menu-checkbox';

            const input = document.createElement('input');
            input.type = 'checkbox';
            input.checked = options.checked || false;

            const box = document.createElement('span');
            box.className = 'menu-checkbox-box';

            const text = document.createElement('span');
            text.className = 'menu-checkbox-label';
            text.textContent = options.label;

            input.addEventListener('change', () => {
                if (options.onChange) options.onChange(input.checked);
                MenuAudio.playSound('checkbox_toggle');
            });

            label.appendChild(input);
            label.appendChild(box);
            label.appendChild(text);

            return { container: label, input };
        },

        /**
         * Create a list item
         */
        createListItem: function(options) {
            const item = document.createElement('div');
            item.className = 'menu-list-item';
            if (options.selected) item.classList.add('selected');

            if (options.icon) {
                const icon = document.createElement('img');
                icon.className = 'menu-list-item-icon';
                icon.src = options.icon;
                item.appendChild(icon);
            }

            const content = document.createElement('div');
            content.className = 'menu-list-item-content';

            const title = document.createElement('div');
            title.className = 'menu-list-item-title';
            title.textContent = options.title;
            content.appendChild(title);

            if (options.subtitle) {
                const subtitle = document.createElement('div');
                subtitle.className = 'menu-list-item-subtitle';
                subtitle.textContent = options.subtitle;
                content.appendChild(subtitle);
            }

            item.appendChild(content);

            if (options.badge) {
                const badge = document.createElement('span');
                badge.className = 'menu-list-item-badge';
                badge.textContent = options.badge;
                item.appendChild(badge);
            }

            item.addEventListener('click', () => {
                if (options.onClick) options.onClick();
                MenuAudio.playSound('list_select');
            });

            return item;
        },

        /**
         * Create progress bar
         */
        createProgressBar: function(options = {}) {
            const container = document.createElement('div');
            container.className = 'menu-progress' + (options.large ? ' menu-progress-lg' : '');

            const fill = document.createElement('div');
            fill.className = 'menu-progress-fill';
            fill.style.width = (options.value || 0) + '%';

            const text = document.createElement('span');
            text.className = 'menu-progress-text';
            text.textContent = options.text || `${options.value || 0}%`;

            container.appendChild(fill);
            container.appendChild(text);

            return {
                container,
                fill,
                text,
                setValue: (value, textContent) => {
                    fill.style.width = value + '%';
                    text.textContent = textContent || `${value}%`;
                }
            };
        },

        /**
         * Create race card for campaign selection
         */
        createRaceCard: function(race) {
            const card = document.createElement('div');
            card.className = 'race-card';
            if (race.locked) card.classList.add('locked');
            if (race.selected) card.classList.add('selected');
            card.dataset.raceId = race.id;

            card.innerHTML = `
                <div class="race-icon">
                    <img src="${race.icon}" alt="${race.name}">
                </div>
                <div class="race-name">${race.name}</div>
                <div class="race-description">${race.description}</div>
                <div class="race-progress">
                    <div class="menu-progress">
                        <div class="menu-progress-fill" style="width: ${race.progress}%"></div>
                        <span class="menu-progress-text">${race.progress}%</span>
                    </div>
                </div>
            `;

            if (!race.locked) {
                card.addEventListener('click', () => {
                    document.querySelectorAll('.race-card').forEach(c => c.classList.remove('selected'));
                    card.classList.add('selected');
                    MenuAudio.playSound('race_select');
                    Engine.call('Campaign.onRaceSelect', { raceId: race.id });
                });
            }

            return card;
        },

        /**
         * Show modal dialog
         */
        showModal: function(options) {
            const overlay = document.createElement('div');
            overlay.className = 'menu-modal-overlay';
            overlay.style.cssText = `
                position: fixed;
                top: 0;
                left: 0;
                width: 100%;
                height: 100%;
                background: rgba(0, 0, 0, 0.7);
                display: flex;
                align-items: center;
                justify-content: center;
                z-index: 2000;
            `;

            const modal = document.createElement('div');
            modal.className = 'menu-panel menu-panel-ornate';
            modal.style.cssText = `
                min-width: 400px;
                max-width: 600px;
                animation: menu-scale-in 0.3s ease-out;
            `;

            modal.innerHTML = `
                <div class="menu-panel-header">
                    <h3 class="menu-panel-title">${options.title}</h3>
                    ${options.showClose !== false ? '<button class="menu-panel-close">&times;</button>' : ''}
                </div>
                <div class="menu-panel-content">
                    ${options.content}
                </div>
                ${options.buttons ? `
                    <div class="menu-panel-footer menu-flex-center menu-gap-md menu-mt-lg">
                        ${options.buttons.map(btn =>
                            `<button class="menu-btn ${btn.primary ? 'menu-btn-primary' : ''} ${btn.danger ? 'menu-btn-danger' : ''} menu-btn-sm" data-action="${btn.action}">${btn.text}</button>`
                        ).join('')}
                    </div>
                ` : ''}
            `;

            overlay.appendChild(modal);
            document.body.appendChild(overlay);

            // Event handlers
            const closeModal = (result) => {
                overlay.style.opacity = '0';
                setTimeout(() => {
                    overlay.remove();
                    if (options.onClose) options.onClose(result);
                }, 200);
            };

            if (options.showClose !== false) {
                modal.querySelector('.menu-panel-close').addEventListener('click', () => closeModal('close'));
            }

            if (options.closeOnOverlay !== false) {
                overlay.addEventListener('click', (e) => {
                    if (e.target === overlay) closeModal('overlay');
                });
            }

            if (options.buttons) {
                modal.querySelectorAll('[data-action]').forEach(btn => {
                    btn.addEventListener('click', () => closeModal(btn.dataset.action));
                });
            }

            MenuAudio.playSound('modal_open');

            return { overlay, modal, close: closeModal };
        }
    };

    // ===== Loading Tips =====
    const LoadingTips = {
        _tips: [
            "Build workers early to boost your economy.",
            "Scout your enemy to learn their strategy.",
            "Control resource points to gain map advantage.",
            "Use hotkeys for faster unit production.",
            "Mix unit types to counter your opponent.",
            "Protect your siege units with infantry.",
            "Research upgrades to strengthen your army.",
            "Use terrain to your advantage in battles.",
            "Build multiple production buildings for faster armies.",
            "Keep your hero alive - they provide powerful abilities.",
            "Flanking attacks deal bonus damage.",
            "Night battles reduce vision range for all units.",
            "Naval units can transport land forces.",
            "Walls slow enemy advances significantly.",
            "Markets allow trading resources with allies."
        ],

        /**
         * Get a random tip
         */
        getRandomTip: function() {
            return this._tips[Math.floor(Math.random() * this._tips.length)];
        },

        /**
         * Add custom tips
         */
        addTips: function(tips) {
            this._tips = this._tips.concat(tips);
        },

        /**
         * Start rotating tips
         */
        startRotation: function(element, interval = 5000) {
            element.textContent = this.getRandomTip();
            element.style.opacity = '1';

            this._rotationInterval = setInterval(() => {
                element.style.opacity = '0';
                setTimeout(() => {
                    element.textContent = this.getRandomTip();
                    element.style.opacity = '1';
                }, 300);
            }, interval);
        },

        /**
         * Stop rotation
         */
        stopRotation: function() {
            if (this._rotationInterval) {
                clearInterval(this._rotationInterval);
                this._rotationInterval = null;
            }
        }
    };

    // ===== Export =====
    global.MenuCore = {
        State: MenuState,
        Navigation: MenuNavigation,
        Animations: MenuAnimations,
        Parallax: ParallaxBackground,
        Audio: MenuAudio,
        Backend: MenuBackend,
        Components: MenuComponents,
        Tips: LoadingTips,

        /**
         * Initialize all menu systems
         */
        init: function() {
            MenuNavigation.init();
            MenuAudio.init();

            // Initialize parallax if container exists
            const parallaxContainer = document.querySelector('.parallax-container');
            if (parallaxContainer) {
                ParallaxBackground.init(parallaxContainer);
            }

            console.log('MenuCore initialized');
        }
    };

    // Shortcuts
    global.MenuNav = MenuNavigation;
    global.MenuAnim = MenuAnimations;

})(window);
