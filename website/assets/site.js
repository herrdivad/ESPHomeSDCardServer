'use strict';

// Core content and navigation remain available without JavaScript.
document.documentElement.classList.add('js');
const menu = document.querySelector('.menu-toggle');
const navigation = document.querySelector('#navigation');
if (menu && navigation) {
  menu.hidden = false;
  const closeMenu = () => {
    menu.setAttribute('aria-expanded', 'false');
    navigation.classList.remove('is-open');
  };
  menu.addEventListener('click', () => {
    const expanded = menu.getAttribute('aria-expanded') !== 'true';
    menu.setAttribute('aria-expanded', String(expanded));
    navigation.classList.toggle('is-open', expanded);
  });
  navigation.addEventListener('click', event => {
    if (event.target.closest('a')) closeMenu();
  });
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && menu.getAttribute('aria-expanded') === 'true') {
      closeMenu();
      menu.focus();
    }
  });
}

if (navigator.clipboard && window.isSecureContext) {
  document.querySelectorAll('.code-block').forEach(block => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'copy-button';
    button.textContent = 'Copy';
    button.setAttribute('aria-label', `Copy ${block.querySelector('.code-label span').textContent}`);
    block.querySelector('.code-label').append(button);
    button.addEventListener('click', async () => {
      const status = document.querySelector('#copy-status');
      try {
        await navigator.clipboard.writeText(block.querySelector('code').textContent);
        button.textContent = 'Copied';
        status.textContent = 'Code copied to clipboard.';
      } catch {
        status.textContent = 'Copy was unavailable. Select the code and copy it manually.';
      }
      window.setTimeout(() => { button.textContent = 'Copy'; }, 2000);
    });
  });
}

const dialog = document.querySelector('.image-dialog');
if (dialog && typeof dialog.showModal === 'function') {
  let trigger;
  const image = dialog.querySelector('img');
  document.querySelectorAll('[data-lightbox]').forEach(link => {
    link.addEventListener('click', event => {
      if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey || event.button !== 0) return;
      event.preventDefault();
      trigger = link;
      image.src = link.href;
      image.alt = link.querySelector('img').alt;
      dialog.querySelector('#image-caption').textContent = link.dataset.caption;
      dialog.showModal();
      document.body.classList.add('dialog-open');
    });
  });
  dialog.querySelector('button').addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', event => {
    const bounds = dialog.getBoundingClientRect();
    if (event.target === dialog && (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom)) dialog.close();
  });
  dialog.addEventListener('close', () => {
    document.body.classList.remove('dialog-open');
    trigger?.focus({preventScroll: true});
  });
}
