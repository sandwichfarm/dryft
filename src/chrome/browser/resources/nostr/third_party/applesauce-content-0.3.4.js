// applesauce-content v0.3.4
// Placeholder for Tungsten
// Full library: https://github.com/hzrd149/applesauce

export const version = '0.3.4';

// Content parsing functionality
export function parseContent(content) {
  // Parse mentions, hashtags, links etc
  const mentions = [];
  const hashtags = [];
  const links = [];
  
  // Simple pattern matching
  const mentionRegex = /nostr:npub[a-zA-Z0-9]+/g;
  const hashtagRegex = /#[a-zA-Z0-9_]+/g;
  const linkRegex = /https?:\/\/[^\s]+/g;
  
  let match;
  while ((match = mentionRegex.exec(content)) !== null) {
    mentions.push({ text: match[0], start: match.index, end: match.index + match[0].length });
  }
  
  while ((match = hashtagRegex.exec(content)) !== null) {
    hashtags.push({ text: match[0], start: match.index, end: match.index + match[0].length });
  }
  
  while ((match = linkRegex.exec(content)) !== null) {
    links.push({ text: match[0], start: match.index, end: match.index + match[0].length });
  }
  
  return {
    content,
    mentions,
    hashtags,
    links
  };
}

// Render content with transformations
export function renderContent(content, options = {}) {
  let rendered = content;
  
  if (options.renderMentions) {
    rendered = rendered.replace(/nostr:npub[a-zA-Z0-9]+/g, (match) => {
      return `<a href="${match}">${match}</a>`;
    });
  }
  
  if (options.renderHashtags) {
    rendered = rendered.replace(/#[a-zA-Z0-9_]+/g, (match) => {
      return `<a href="/t/${match.slice(1)}">${match}</a>`;
    });
  }
  
  if (options.renderLinks) {
    rendered = rendered.replace(/https?:\/\/[^\s]+/g, (match) => {
      return `<a href="${match}" target="_blank">${match}</a>`;
    });
  }
  
  return rendered;
}

export default {
  version,
  parseContent,
  renderContent
};