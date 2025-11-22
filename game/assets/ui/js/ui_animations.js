/**
 * UI Animations - Animation triggers and effects
 */

(function(global) {
    'use strict';

    // ===== Animation Controller =====
    const AnimationController = {
        _active: new Map(),
        _queue: new Map(),

        /**
         * Play animation on element
         */
        play: function(el, animationName, options = {}) {
            if (typeof el === 'string') {
                el = document.querySelector(el);
            }
            if (!el) return Promise.resolve();

            const {
                duration = null,
                delay = 0,
                easing = null,
                fill = 'forwards',
                onStart = null,
                onComplete = null
            } = options;

            return new Promise((resolve) => {
                // Cancel any existing animation
                this.stop(el);

                setTimeout(() => {
                    if (onStart) onStart(el);

                    // Apply animation class
                    el.classList.add(`animate-${animationName}`);

                    // Override duration/easing if specified
                    if (duration) el.style.animationDuration = `${duration}ms`;
                    if (easing) el.style.animationTimingFunction = easing;
                    el.style.animationFillMode = fill;

                    const handleEnd = () => {
                        el.removeEventListener('animationend', handleEnd);
                        this._active.delete(el);

                        if (fill !== 'forwards') {
                            el.classList.remove(`animate-${animationName}`);
                        }

                        if (onComplete) onComplete(el);
                        resolve(el);
                    };

                    el.addEventListener('animationend', handleEnd);
                    this._active.set(el, { animationName, handleEnd });
                }, delay);
            });
        },

        /**
         * Stop animation on element
         */
        stop: function(el) {
            if (typeof el === 'string') {
                el = document.querySelector(el);
            }
            if (!el) return;

            const active = this._active.get(el);
            if (active) {
                el.removeEventListener('animationend', active.handleEnd);
                el.classList.remove(`animate-${active.animationName}`);
                el.style.animation = '';
                this._active.delete(el);
            }
        },

        /**
         * Chain multiple animations
         */
        sequence: async function(animations) {
            for (const anim of animations) {
                await this.play(anim.el, anim.name, anim.options);
            }
        },

        /**
         * Play animations in parallel
         */
        parallel: function(animations) {
            return Promise.all(
                animations.map(anim => this.play(anim.el, anim.name, anim.options))
            );
        },

        /**
         * Queue animation (wait for previous to finish)
         */
        queue: function(el, animationName, options = {}) {
            if (typeof el === 'string') {
                el = document.querySelector(el);
            }
            if (!el) return Promise.resolve();

            if (!this._queue.has(el)) {
                this._queue.set(el, Promise.resolve());
            }

            const queuePromise = this._queue.get(el).then(() => {
                return this.play(el, animationName, options);
            });

            this._queue.set(el, queuePromise);
            return queuePromise;
        }
    };

    // ===== Transition Helper =====
    const Transitions = {
        /**
         * Transition property with animation
         */
        to: function(el, properties, options = {}) {
            if (typeof el === 'string') {
                el = document.querySelector(el);
            }
            if (!el) return Promise.resolve();

            const {
                duration = 300,
                easing = 'ease',
                delay = 0
            } = options;

            return new Promise((resolve) => {
                const transitionProps = Object.keys(properties).join(', ');
                el.style.transition = `${transitionProps} ${duration}ms ${easing} ${delay}ms`;

                // Apply properties
                Object.entries(properties).forEach(([prop, value]) => {
                    el.style[prop] = value;
                });

                const handleEnd = () => {
                    el.removeEventListener('transitionend', handleEnd);
                    resolve(el);
                };

                el.addEventListener('transitionend', handleEnd);

                // Fallback timeout
                setTimeout(() => {
                    el.removeEventListener('transitionend', handleEnd);
                    resolve(el);
                }, duration + delay + 50);
            });
        },

        /**
         * Fade in element
         */
        fadeIn: function(el, duration = 300) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            el.style.opacity = '0';
            el.style.display = '';

            return this.to(el, { opacity: 1 }, { duration });
        },

        /**
         * Fade out element
         */
        fadeOut: function(el, duration = 300) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            return this.to(el, { opacity: 0 }, { duration }).then(() => {
                el.style.display = 'none';
                return el;
            });
        },

        /**
         * Slide down (show)
         */
        slideDown: function(el, duration = 300) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            el.style.display = '';
            const height = el.scrollHeight;
            el.style.height = '0';
            el.style.overflow = 'hidden';

            return this.to(el, { height: height + 'px' }, { duration }).then(() => {
                el.style.height = '';
                el.style.overflow = '';
                return el;
            });
        },

        /**
         * Slide up (hide)
         */
        slideUp: function(el, duration = 300) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            el.style.height = el.scrollHeight + 'px';
            el.style.overflow = 'hidden';

            // Force reflow
            el.offsetHeight;

            return this.to(el, { height: '0' }, { duration }).then(() => {
                el.style.display = 'none';
                el.style.height = '';
                el.style.overflow = '';
                return el;
            });
        },

        /**
         * Toggle slide
         */
        slideToggle: function(el, duration = 300) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            return el.offsetHeight === 0 || el.style.display === 'none'
                ? this.slideDown(el, duration)
                : this.slideUp(el, duration);
        }
    };

    // ===== Effect Triggers =====
    const Effects = {
        /**
         * Shake effect
         */
        shake: function(el, intensity = 5, duration = 500) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return;

            const startTime = performance.now();
            const originalTransform = el.style.transform;

            const tick = (currentTime) => {
                const elapsed = currentTime - startTime;
                const progress = elapsed / duration;

                if (progress < 1) {
                    const decay = 1 - progress;
                    const x = (Math.random() - 0.5) * 2 * intensity * decay;
                    const y = (Math.random() - 0.5) * 2 * intensity * decay;
                    el.style.transform = `${originalTransform} translate(${x}px, ${y}px)`;
                    requestAnimationFrame(tick);
                } else {
                    el.style.transform = originalTransform;
                }
            };

            requestAnimationFrame(tick);
        },

        /**
         * Flash effect
         */
        flash: function(el, color = 'rgba(255, 255, 255, 0.5)', duration = 200) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return;

            const overlay = document.createElement('div');
            overlay.style.cssText = `
                position: absolute;
                top: 0; left: 0; right: 0; bottom: 0;
                background: ${color};
                pointer-events: none;
                z-index: 1000;
            `;

            el.style.position = 'relative';
            el.appendChild(overlay);

            Transitions.fadeOut(overlay, duration).then(() => overlay.remove());
        },

        /**
         * Pulse effect
         */
        pulse: function(el, scale = 1.1, duration = 200) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            return Transitions.to(el, { transform: `scale(${scale})` }, { duration: duration / 2 })
                .then(() => Transitions.to(el, { transform: 'scale(1)' }, { duration: duration / 2 }));
        },

        /**
         * Ripple effect (Material Design style)
         */
        ripple: function(el, x, y, color = 'rgba(255, 255, 255, 0.3)') {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return;

            const rect = el.getBoundingClientRect();
            const size = Math.max(rect.width, rect.height) * 2;

            const ripple = document.createElement('div');
            ripple.style.cssText = `
                position: absolute;
                width: ${size}px;
                height: ${size}px;
                background: ${color};
                border-radius: 50%;
                transform: scale(0);
                pointer-events: none;
                left: ${x - rect.left - size / 2}px;
                top: ${y - rect.top - size / 2}px;
            `;

            el.style.position = 'relative';
            el.style.overflow = 'hidden';
            el.appendChild(ripple);

            Transitions.to(ripple, {
                transform: 'scale(1)',
                opacity: 0
            }, { duration: 600 }).then(() => ripple.remove());
        },

        /**
         * Number counter animation
         */
        countTo: function(el, targetValue, options = {}) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return;

            const {
                duration = 1000,
                decimals = 0,
                prefix = '',
                suffix = '',
                separator = ',',
                easing = 'easeOutQuad'
            } = options;

            const startValue = parseFloat(el.textContent.replace(/[^0-9.-]/g, '')) || 0;
            const startTime = performance.now();

            const formatNumber = (num) => {
                const fixed = num.toFixed(decimals);
                const [integer, decimal] = fixed.split('.');
                const formatted = integer.replace(/\B(?=(\d{3})+(?!\d))/g, separator);
                return prefix + (decimal ? `${formatted}.${decimal}` : formatted) + suffix;
            };

            const ease = (t) => {
                if (easing === 'easeOutQuad') return t * (2 - t);
                if (easing === 'easeInOutQuad') return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
                return t;
            };

            const tick = (currentTime) => {
                const elapsed = currentTime - startTime;
                const progress = Math.min(elapsed / duration, 1);
                const easedProgress = ease(progress);
                const currentValue = startValue + (targetValue - startValue) * easedProgress;

                el.textContent = formatNumber(currentValue);

                if (progress < 1) {
                    requestAnimationFrame(tick);
                }
            };

            requestAnimationFrame(tick);
        },

        /**
         * Typewriter effect
         */
        typewrite: function(el, text, options = {}) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            const {
                speed = 50,
                cursor = true,
                cursorChar = '|',
                onChar = null
            } = options;

            return new Promise((resolve) => {
                el.textContent = '';
                let i = 0;

                if (cursor) {
                    const cursorEl = document.createElement('span');
                    cursorEl.className = 'typewriter-cursor';
                    cursorEl.textContent = cursorChar;
                    el.appendChild(cursorEl);
                }

                const type = () => {
                    if (i < text.length) {
                        const char = text.charAt(i);
                        const textNode = document.createTextNode(char);

                        if (cursor) {
                            el.insertBefore(textNode, el.lastChild);
                        } else {
                            el.appendChild(textNode);
                        }

                        if (onChar) onChar(char, i);
                        i++;
                        setTimeout(type, speed);
                    } else {
                        resolve(el);
                    }
                };

                type();
            });
        },

        /**
         * Scramble text effect
         */
        scramble: function(el, finalText, options = {}) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el) return Promise.resolve();

            const {
                duration = 1000,
                chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
            } = options;

            return new Promise((resolve) => {
                const startTime = performance.now();
                const originalText = el.textContent;
                const maxLength = Math.max(originalText.length, finalText.length);

                const tick = (currentTime) => {
                    const elapsed = currentTime - startTime;
                    const progress = Math.min(elapsed / duration, 1);
                    const revealCount = Math.floor(progress * finalText.length);

                    let result = '';
                    for (let i = 0; i < maxLength; i++) {
                        if (i < revealCount) {
                            result += finalText[i] || '';
                        } else if (i < finalText.length) {
                            result += chars[Math.floor(Math.random() * chars.length)];
                        }
                    }

                    el.textContent = result;

                    if (progress < 1) {
                        requestAnimationFrame(tick);
                    } else {
                        el.textContent = finalText;
                        resolve(el);
                    }
                };

                requestAnimationFrame(tick);
            });
        }
    };

    // ===== Scroll Animations =====
    const ScrollAnimations = {
        _observer: null,
        _observed: new Map(),

        /**
         * Initialize scroll observer
         */
        init: function(options = {}) {
            const {
                threshold = 0.1,
                rootMargin = '0px'
            } = options;

            this._observer = new IntersectionObserver((entries) => {
                entries.forEach(entry => {
                    const config = this._observed.get(entry.target);
                    if (!config) return;

                    if (entry.isIntersecting) {
                        if (config.onEnter) config.onEnter(entry.target);
                        if (config.animation) {
                            AnimationController.play(entry.target, config.animation, config.animationOptions);
                        }
                        if (config.once) {
                            this.unobserve(entry.target);
                        }
                    } else {
                        if (config.onLeave) config.onLeave(entry.target);
                    }
                });
            }, { threshold, rootMargin });

            return this;
        },

        /**
         * Observe element for scroll animation
         */
        observe: function(el, config) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el || !this._observer) return;

            this._observed.set(el, config);
            this._observer.observe(el);
        },

        /**
         * Stop observing element
         */
        unobserve: function(el) {
            if (typeof el === 'string') el = document.querySelector(el);
            if (!el || !this._observer) return;

            this._observer.unobserve(el);
            this._observed.delete(el);
        },

        /**
         * Observe all elements with data-scroll-animate
         */
        observeAll: function(container = document) {
            container.querySelectorAll('[data-scroll-animate]').forEach(el => {
                const animation = el.dataset.scrollAnimate;
                const once = el.dataset.scrollOnce !== 'false';
                const delay = parseInt(el.dataset.scrollDelay) || 0;

                this.observe(el, {
                    animation,
                    once,
                    animationOptions: { delay }
                });
            });
        }
    };

    // ===== Export =====
    global.UIAnimations = {
        AnimationController,
        Transitions,
        Effects,
        ScrollAnimations,

        // Shortcuts
        play: AnimationController.play.bind(AnimationController),
        stop: AnimationController.stop.bind(AnimationController),
        fadeIn: Transitions.fadeIn.bind(Transitions),
        fadeOut: Transitions.fadeOut.bind(Transitions),
        shake: Effects.shake.bind(Effects),
        pulse: Effects.pulse.bind(Effects)
    };

    // Initialize scroll animations
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            ScrollAnimations.init().observeAll();
        });
    } else {
        ScrollAnimations.init().observeAll();
    }

})(window);
