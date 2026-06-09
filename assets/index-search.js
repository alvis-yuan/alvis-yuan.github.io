(function () {
  'use strict';

  var INDEX_URL = 'assets/search-index.json';
  var DEBOUNCE_MS = 120;
  var MAX_RESULTS = 24;

  var input = document.getElementById('site-search-input');
  var resultsEl = document.getElementById('site-search-results');
  var metaEl = document.getElementById('site-search-meta');
  if (!input || !resultsEl || !metaEl) {
    return;
  }

  var entries = [];
  var activeIndex = -1;
  var debounceTimer = null;
  var loadError = '';

  function normalize(text) {
    return String(text || '')
      .toLowerCase()
      .replace(/\s+/g, ' ')
      .trim();
  }

  function tokenize(query) {
    return normalize(query).split(/[\s/_.-]+/).filter(Boolean);
  }

  function subsequenceScore(query, text) {
    var qi = 0;
    var score = 0;
    var consecutive = 0;
    var firstIdx = -1;

    for (var i = 0; i < text.length && qi < query.length; i += 1) {
      if (text[i] === query[qi]) {
        if (firstIdx === -1) {
          firstIdx = i;
        }
        score += 12 + consecutive * 4;
        consecutive += 1;
        qi += 1;
      } else {
        consecutive = 0;
      }
    }

    if (qi < query.length) {
      return 0;
    }

    score += Math.max(0, 40 - firstIdx);
    score += Math.max(0, 24 - (text.length - query.length));
    return score;
  }

  function scoreHaystack(haystack, query) {
    var q = normalize(query);
    if (!q) {
      return 0;
    }

    var hay = normalize(haystack);
    if (!hay) {
      return 0;
    }

    if (hay.indexOf(q) !== -1) {
      return 220 + Math.max(0, 180 - hay.indexOf(q));
    }

    var tokens = tokenize(q);
    if (tokens.length > 1) {
      var total = 0;
      for (var i = 0; i < tokens.length; i += 1) {
        var token = tokens[i];
        if (hay.indexOf(token) !== -1) {
          total += 80 + Math.max(0, 60 - hay.indexOf(token));
          continue;
        }
        var partial = subsequenceScore(token, hay);
        if (!partial) {
          return 0;
        }
        total += partial;
      }
      return total + tokens.length * 8;
    }

    return subsequenceScore(q, hay);
  }

  function escapeHtml(text) {
    return String(text)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function highlight(text, query) {
    var q = normalize(query);
    if (!q) {
      return escapeHtml(text);
    }

    var lower = String(text).toLowerCase();
    var idx = lower.indexOf(q);
    if (idx !== -1) {
      return escapeHtml(text.slice(0, idx))
        + '<mark class="site-search-mark">' + escapeHtml(text.slice(idx, idx + q.length)) + '</mark>'
        + escapeHtml(text.slice(idx + q.length));
    }

    return escapeHtml(text);
  }

  function searchEntries(query) {
    if (!normalize(query)) {
      return [];
    }

    return entries
      .map(function (entry) {
        return {
          entry: entry,
          score: scoreHaystack(entry.haystack || (entry.title + ' ' + entry.href), query)
        };
      })
      .filter(function (item) {
        return item.score > 0;
      })
      .sort(function (a, b) {
        if (b.score !== a.score) {
          return b.score - a.score;
        }
        return a.entry.title.localeCompare(b.entry.title, 'zh-CN');
      })
      .slice(0, MAX_RESULTS);
  }

  function filterPageSections(query) {
    var q = normalize(query);
    var blocks = document.querySelectorAll('section, nav');

    blocks.forEach(function (block) {
      if (!q) {
        block.hidden = false;
        block.querySelectorAll('li').forEach(function (li) {
          li.hidden = false;
        });
        return;
      }

      var blockText = normalize(block.textContent);
      var blockMatch = scoreHaystack(blockText, query) > 0;
      var visibleItems = 0;

      block.querySelectorAll('li').forEach(function (li) {
        var liMatch = scoreHaystack(normalize(li.textContent), query) > 0;
        li.hidden = !liMatch && !blockMatch;
        if (!li.hidden) {
          visibleItems += 1;
        }
      });

      block.hidden = !blockMatch && visibleItems === 0;
    });
  }

  function setExpanded(open) {
    input.setAttribute('aria-expanded', open ? 'true' : 'false');
    resultsEl.hidden = !open;
  }

  function renderResults(matches, query) {
    activeIndex = -1;
    resultsEl.innerHTML = '';

    if (!normalize(query)) {
      setExpanded(false);
      metaEl.textContent = '输入关键词进行模糊搜索，支持标题、路径、模块名与拼音分隔符。';
      filterPageSections('');
      return;
    }

    if (loadError) {
      metaEl.textContent = loadError;
      setExpanded(false);
      filterPageSections(query);
      return;
    }

    if (!matches.length) {
      metaEl.textContent = '未找到匹配结果。';
      setExpanded(false);
      filterPageSections(query);
      return;
    }

    metaEl.textContent = '找到 ' + matches.length + ' 条结果（最多显示 ' + MAX_RESULTS + ' 条）。';
    setExpanded(true);

    matches.forEach(function (item, index) {
      var entry = item.entry;
      var li = document.createElement('li');
      li.className = 'site-search-item';
      li.setAttribute('role', 'option');

      var link = document.createElement('a');
      link.className = 'site-search-link';
      link.href = entry.href;
      link.setAttribute('data-index', String(index));
      link.innerHTML =
        '<span class="site-search-badge">' + highlight(entry.moduleTitle || entry.module || '文档', query) + '</span>'
        + '<span class="site-search-title">' + highlight(entry.title, query) + '</span>'
        + '<span class="site-search-path">' + highlight(entry.href, query) + '</span>';

      li.appendChild(link);
      resultsEl.appendChild(li);
    });

    filterPageSections(query);
  }

  function setActiveResult(index) {
    var links = resultsEl.querySelectorAll('.site-search-link');
    links.forEach(function (link, i) {
      link.setAttribute('data-active', i === index ? 'true' : 'false');
    });
    activeIndex = index;
    if (index >= 0 && links[index]) {
      links[index].scrollIntoView({ block: 'nearest' });
    }
  }

  function runSearch() {
    var query = input.value;
    renderResults(searchEntries(query), query);
  }

  function scheduleSearch() {
    window.clearTimeout(debounceTimer);
    debounceTimer = window.setTimeout(runSearch, DEBOUNCE_MS);
  }

  input.addEventListener('input', scheduleSearch);

  input.addEventListener('keydown', function (event) {
    var links = resultsEl.querySelectorAll('.site-search-link');
    if (!links.length) {
      if (event.key === 'Escape') {
        input.value = '';
        runSearch();
      }
      return;
    }

    if (event.key === 'ArrowDown') {
      event.preventDefault();
      setActiveResult(Math.min(activeIndex + 1, links.length - 1));
      return;
    }

    if (event.key === 'ArrowUp') {
      event.preventDefault();
      setActiveResult(Math.max(activeIndex - 1, 0));
      return;
    }

    if (event.key === 'Enter' && activeIndex >= 0) {
      event.preventDefault();
      window.location.href = links[activeIndex].href;
      return;
    }

    if (event.key === 'Escape') {
      input.value = '';
      runSearch();
      input.blur();
    }
  });

  document.addEventListener('click', function (event) {
    if (!event.target.closest('.site-search')) {
      setExpanded(false);
    }
  });

  function loadSearchIndex() {
    var dataNode = document.getElementById('site-search-data');
    if (dataNode && dataNode.textContent) {
      try {
        var inline = JSON.parse(dataNode.textContent);
        if (Array.isArray(inline)) {
          entries = inline;
          metaEl.textContent = '已索引 ' + entries.length + ' 篇文档，输入关键词开始搜索。';
          return;
        }
      } catch (err) {
        // fall through to fetch
      }
    }

    metaEl.textContent = '正在加载搜索索引…';
    fetch(INDEX_URL)
      .then(function (response) {
        if (!response.ok) {
          throw new Error('无法加载搜索索引');
        }
        return response.json();
      })
      .then(function (data) {
        entries = Array.isArray(data) ? data : [];
        metaEl.textContent = '已索引 ' + entries.length + ' 篇文档，输入关键词开始搜索。';
      })
      .catch(function () {
        loadError = '搜索索引加载失败，仍可过滤本页模块列表。';
        metaEl.textContent = loadError;
      });
  }

  loadSearchIndex();
})();
