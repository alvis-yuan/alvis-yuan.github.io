(function () {
  if (window.__siteNavEnhancerLoaded) {
    return;
  }
  window.__siteNavEnhancerLoaded = true;

  var PANEL_ID = 'site-nav-drawer';
  var TOGGLE_ID = 'site-nav-toggle';
  var OVERLAY_ID = 'site-nav-overlay';
  var LEGACY_NAV_SELECTORS = [
    '#sidebar',
    'nav#sidebar',
    'aside#sidebar',
    '.sidebar',
    'nav.sidebar',
    'aside.sidebar',
    '#packet-sidebar',
    'nav#packet-sidebar',
    'aside#packet-sidebar',
    'nav.packet-sidebar',
    'aside.packet-sidebar',
    'div#toc',
    '#btNavOverlay',
    '.bt-nav-overlay'
  ];
  var LEGACY_BUTTON_SELECTORS = [
    '#hamburger',
    '#hamBtn',
    '#packetHamBtn',
    '#menu-btn',
    '#menu-toggle'
  ];
  var LEGACY_OVERLAY_SELECTORS = [
    '#overlay',
    '#packet-overlay'
  ];

  function parsePixels(value) {
    var parsed = Number.parseFloat(value || '0');
    return Number.isFinite(parsed) ? parsed : 0;
  }

  function removeMatches(selectors) {
    document.querySelectorAll(selectors.join(',')).forEach(function (node) {
      node.remove();
    });
  }

  function setStyleIfLarge(selector, propertyName, maxAllowed, value) {
    document.querySelectorAll(selector).forEach(function (node) {
      var style = window.getComputedStyle(node);
      if (parsePixels(style.getPropertyValue(propertyName)) > maxAllowed) {
        node.style.setProperty(propertyName, value, 'important');
      }
    });
  }

  function cleanupLegacyNavigation() {
    var hasLegacyNavigation = Boolean(document.querySelector(
      LEGACY_NAV_SELECTORS.concat(LEGACY_BUTTON_SELECTORS, LEGACY_OVERLAY_SELECTORS).join(',')
    ));

    if (!hasLegacyNavigation) {
      return;
    }

    removeMatches(LEGACY_NAV_SELECTORS);
    removeMatches(LEGACY_BUTTON_SELECTORS);
    removeMatches(LEGACY_OVERLAY_SELECTORS);

    setStyleIfLarge('.page, .packet-page, #main', 'margin-left', 72, '0px');
    setStyleIfLarge('#topbar, .top-bar, .packet-top-bar', 'padding-left', 72, '16px');
    setStyleIfLarge('.index-link, .bt-spec-index-link', 'left', 72, '18px');
  }

  function slugify(text) {
    var cleaned = String(text || '')
      .trim()
      .toLowerCase()
      .replace(/[\s\u3000]+/g, '-')
      .replace(/[^\w\-\u4e00-\u9fa5]+/g, '-')
      .replace(/-+/g, '-')
      .replace(/^-|-$/g, '');
    return cleaned || 'section';
  }

  function ensureUniqueId(base, seen) {
    var id = base;
    var index = 2;
    while (seen[id] || document.getElementById(id)) {
      id = base + '-' + index;
      index += 1;
    }
    seen[id] = true;
    return id;
  }

  function getHeadingText(node) {
    return (node.textContent || '')
      .replace(/\s+/g, ' ')
      .trim();
  }

  function getHeadingLevel(node) {
    if (/^H[1-6]$/.test(node.tagName)) {
      return Number(node.tagName.slice(1));
    }
    if (node.classList.contains('section-title') || node.classList.contains('sec-title')) {
      return 2;
    }
    if (node.classList.contains('section-subtitle') || node.classList.contains('subsection-title') || node.classList.contains('sub-title')) {
      return 3;
    }
    if (node.classList.contains('subsub-title')) {
      return 4;
    }
    return 3;
  }

  function findAnchorNode(node, level) {
    if (node.id) {
      return node;
    }

    var ancestor = node;
    while (ancestor && ancestor !== document.body) {
      var previous = ancestor.previousElementSibling;
      while (previous) {
        if (previous.id && (previous.classList.contains('section-anchor') || previous.tagName === 'A')) {
          return previous;
        }
        previous = previous.previousElementSibling;
      }
      ancestor = ancestor.parentElement;
    }

    var sectionAnchor = level <= 2 ? node.closest('section[id], .section[id], .spec-section[id], article[id]') : null;
    if (sectionAnchor && sectionAnchor.id) {
      return sectionAnchor;
    }

    var container = node.closest('.spec-section, .section, section, article, [data-section-id]');
    if (container) {
      if (container.id) {
        return container;
      }
      var explicitAnchor = container.querySelector('.section-anchor[id], [data-site-nav-source-anchor][id]');
      if (explicitAnchor && explicitAnchor.id) {
        return explicitAnchor;
      }
    }

    var sibling = node.previousElementSibling;
    while (sibling) {
      if (sibling.id && (sibling.classList.contains('section-anchor') || sibling.tagName === 'A')) {
        return sibling;
      }
      sibling = sibling.previousElementSibling;
    }

    return node;
  }

  function isInsideExcludedArea(node) {
    return Boolean(node.closest(
      'nav, aside, header, footer, .site-nav-drawer, .sidebar, #sidebar, .top-nav, .top-bar, .packet-sidebar'
    ));
  }

  function isVisibleForNav(node) {
    if (!(node instanceof HTMLElement)) {
      return false;
    }
    var style = window.getComputedStyle(node);
    return style.display !== 'none' && style.visibility !== 'hidden';
  }

  function collectHeadings() {
    var headingNodes = Array.prototype.slice.call(document.querySelectorAll(
      'h1, h2, h3, h4, .section-title, .section-subtitle, .subsection-title, .sec-title, .sub-title, .subsub-title'
    ));
    var seen = Object.create(null);
    var items = [];

    headingNodes.forEach(function (node) {
      if (isInsideExcludedArea(node)) {
        return;
      }
      var text = getHeadingText(node);
      if (!text) {
        return;
      }
      var level = getHeadingLevel(node);
      if (level === 1 && items.length > 0) {
        return;
      }
      var anchorNode = findAnchorNode(node, level);
      if (!anchorNode.id) {
        anchorNode.id = ensureUniqueId(slugify(text), seen);
      } else {
        seen[anchorNode.id] = true;
      }
      anchorNode.setAttribute('data-site-nav-anchor', 'true');
      items.push({
        id: anchorNode.id,
        text: text,
        level: Math.min(Math.max(level, 1), 3),
        node: anchorNode
      });
    });
    return items;
  }

  function collectFallbackHeadings() {
    var seen = Object.create(null);
    var target = document.querySelector('main, [role="main"], .main-content, .content-wrap, .content, .container, #root, body');
    if (!target) {
      return [];
    }

    var heading = document.querySelector('h1, h2');
    var text = heading ? getHeadingText(heading) : getTitle();
    if (!text) {
      return [];
    }

    if (!target.id) {
      target.id = ensureUniqueId(slugify(text), seen);
    } else {
      seen[target.id] = true;
    }
    target.setAttribute('data-site-nav-anchor', 'true');

    return [{
      id: target.id,
      text: text,
      level: 1,
      node: target
    }];
  }

  function buildTree(items) {
    var roots = [];
    var stack = [];

    items.forEach(function (item) {
      var entry = {
        id: item.id,
        text: item.text,
        level: item.level,
        node: item.node,
        children: []
      };

      while (stack.length && stack[stack.length - 1].level >= entry.level) {
        stack.pop();
      }

      if (stack.length) {
        stack[stack.length - 1].children.push(entry);
      } else {
        roots.push(entry);
      }
      stack.push(entry);
    });

    return roots;
  }

  function renderList(items) {
    var list = document.createElement('ul');
    list.className = items === null || items === void 0 ? '' : 'site-nav-list';

    items.forEach(function (item) {
      var listItem = document.createElement('li');
      listItem.className = 'site-nav-item';

      var link = document.createElement('a');
      link.className = 'site-nav-link';
      link.href = '#' + item.id;
      link.textContent = item.text;
      link.setAttribute('data-level', String(item.level));
      link.setAttribute('data-target-id', item.id);
      listItem.appendChild(link);

      if (item.children.length) {
        var sub = renderList(item.children);
        sub.className = 'site-nav-sublist';
        listItem.appendChild(sub);
      }
      list.appendChild(listItem);
    });

    return list;
  }

  function getTitle() {
    var title = document.title || '';
    if (title) {
      return title;
    }
    var h1 = document.querySelector('h1');
    return h1 ? getHeadingText(h1) : '页面目录';
  }

  function computeScrollOffset() {
    var offset = 72;
    var fixedNodes = Array.prototype.slice.call(document.querySelectorAll('body *'));

    fixedNodes.forEach(function (node) {
      if (!(node instanceof HTMLElement)) {
        return;
      }
      if (node.id === PANEL_ID || node.id === TOGGLE_ID || node.id === OVERLAY_ID) {
        return;
      }
      var style = window.getComputedStyle(node);
      if (style.position !== 'fixed') {
        return;
      }
      var rect = node.getBoundingClientRect();
      if (rect.top > 4 || rect.bottom > window.innerHeight * 0.35 || rect.height <= 0) {
        return;
      }
      offset = Math.max(offset, Math.ceil(rect.bottom) + 18);
    });

    return offset;
  }

  function getScrollRoot() {
    if (typeof window.btGetScrollRoot === 'function') {
      return window.btGetScrollRoot();
    }
    var main = document.getElementById('main-content') || document.getElementById('main');
    if (!main) {
      return window;
    }
    var style = window.getComputedStyle(main);
    if ((style.overflowY === 'auto' || style.overflowY === 'scroll') && main.scrollHeight > main.clientHeight + 2) {
      return main;
    }
    return window;
  }

  function revealTargetIfNeeded(target, id) {
    if (!target || isVisibleForNav(target)) {
      return false;
    }
    if (typeof window.show === 'function' && target.classList.contains('section')) {
      window.show(id);
      return true;
    }
    return false;
  }

  function scrollToTarget(id) {
    var target = document.getElementById(id);
    if (!target) {
      return;
    }
    if (revealTargetIfNeeded(target, id)) {
      window.setTimeout(function () {
        window.location.hash = id;
      }, 120);
      return;
    }
    if (typeof window.btScrollToEl === 'function') {
      window.btScrollToEl(target, computeScrollOffset());
      window.setTimeout(function () {
        window.location.hash = id;
      }, 120);
      return;
    }
    var scrollRoot = getScrollRoot();
    var offset = computeScrollOffset();
    if (scrollRoot && scrollRoot !== window) {
      var rootRect = scrollRoot.getBoundingClientRect();
      var topInRoot = target.getBoundingClientRect().top - rootRect.top + scrollRoot.scrollTop - offset;
      scrollRoot.scrollTo({ top: Math.max(topInRoot, 0), behavior: 'smooth' });
    } else {
      var top = target.getBoundingClientRect().top + window.scrollY - offset;
      window.scrollTo({ top: Math.max(top, 0), behavior: 'smooth' });
    }
    window.setTimeout(function () {
      window.location.hash = id;
    }, 120);
  }

  function init() {
    cleanupLegacyNavigation();

    var mountAttempts = 0;
    var maxAttempts = 20;

    function hasPendingDynamicRoot() {
      return Boolean(document.querySelector('#root:empty, #app:empty, #__next:empty, #app-root:empty'));
    }

    function tryMountNavigation() {
      if (document.getElementById(PANEL_ID) || document.getElementById(TOGGLE_ID)) {
        return;
      }

      cleanupLegacyNavigation();

      var headings = collectHeadings();
      if (!headings.length) {
        if (document.querySelector('iframe')) {
          headings = collectFallbackHeadings();
        } else if (!hasPendingDynamicRoot()) {
          headings = collectFallbackHeadings();
        }
      }
      if (!headings.length) {
        if (mountAttempts >= maxAttempts) {
          headings = collectFallbackHeadings();
        } else {
          mountAttempts += 1;
          window.setTimeout(tryMountNavigation, 250);
          return;
        }
      }

      if (!headings.length) {
        return;
      }

      var tree = buildTree(headings);
      var overlay = document.createElement('div');
      overlay.id = OVERLAY_ID;
      overlay.className = 'site-nav-overlay';
      overlay.setAttribute('data-open', 'false');

      var toggle = document.createElement('button');
      toggle.id = TOGGLE_ID;
      toggle.className = 'site-nav-toggle';
      toggle.type = 'button';
      toggle.setAttribute('aria-controls', PANEL_ID);
      toggle.setAttribute('aria-expanded', 'false');
      toggle.innerHTML = '<span class="site-nav-toggle-icon">≡</span><span>目录</span>';

      var drawer = document.createElement('aside');
      drawer.id = PANEL_ID;
      drawer.className = 'site-nav-drawer';
      drawer.setAttribute('data-open', 'false');
      drawer.innerHTML = '' +
        '<div class="site-nav-head">' +
        '  <div>' +
        '    <p class="site-nav-kicker">Navigation</p>' +
        '    <p class="site-nav-title"></p>' +
        '  </div>' +
        '  <button type="button" class="site-nav-close" aria-label="关闭目录">×</button>' +
        '</div>' +
        '<div class="site-nav-body"></div>';

      drawer.querySelector('.site-nav-title').textContent = getTitle();
      drawer.querySelector('.site-nav-body').appendChild(renderList(tree));

      document.body.appendChild(overlay);
      document.body.appendChild(toggle);
      document.body.appendChild(drawer);
      document.documentElement.classList.add('site-nav-enabled');
      document.body.classList.add('site-nav-enabled-body');

      var isOpen = false;

      function applyOpen(next) {
        isOpen = next;
        drawer.setAttribute('data-open', next ? 'true' : 'false');
        overlay.setAttribute('data-open', next ? 'true' : 'false');
        toggle.setAttribute('aria-expanded', next ? 'true' : 'false');
      }

      function openNav() {
        applyOpen(true);
      }

      function closeNav() {
        applyOpen(false);
      }

      toggle.addEventListener('click', function () {
        applyOpen(!isOpen);
      });
      overlay.addEventListener('click', closeNav);
      drawer.querySelector('.site-nav-close').addEventListener('click', closeNav);

      drawer.addEventListener('click', function (event) {
        var link = event.target.closest('.site-nav-link');
        if (!link) {
          return;
        }
        event.preventDefault();
        scrollToTarget(link.getAttribute('data-target-id'));
        if (window.innerWidth <= 900) {
          closeNav();
        }
      });

      document.addEventListener('keydown', function (event) {
        if (event.key === 'Escape') {
          closeNav();
        }
      });

      var linkMap = Object.create(null);
      Array.prototype.slice.call(drawer.querySelectorAll('.site-nav-link')).forEach(function (link) {
        linkMap[link.getAttribute('data-target-id')] = link;
      });

      function setActive(id) {
        Object.keys(linkMap).forEach(function (key) {
          linkMap[key].setAttribute('data-active', key === id ? 'true' : 'false');
        });
      }

      var activeId = headings[0].id;
      setActive(activeId);
      var scrollRoot = getScrollRoot();

      function refreshActive() {
        var offset = computeScrollOffset() + 8;
        var current = activeId;
        headings.forEach(function (item) {
          if (!isVisibleForNav(item.node)) {
            return;
          }
          var rect = item.node.getBoundingClientRect();
          if (rect.top - offset <= 0) {
            current = item.id;
          }
        });
        if (current !== activeId) {
          activeId = current;
          setActive(current);
        }
      }

      if (scrollRoot && scrollRoot !== window) {
        scrollRoot.addEventListener('scroll', refreshActive, { passive: true });
      } else {
        window.addEventListener('scroll', refreshActive, { passive: true });
      }
      window.addEventListener('resize', refreshActive);

      if (window.location.hash) {
        var hashId = decodeURIComponent(window.location.hash.slice(1));
        if (document.getElementById(hashId)) {
          setTimeout(function () {
            scrollToTarget(hashId);
            setActive(hashId);
          }, 60);
        }
      }
    }

    tryMountNavigation();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init, { once: true });
  } else {
    init();
  }
})();