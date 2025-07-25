// applesauce-core v0.3.4
// Placeholder for dryft
// Full library: https://github.com/hzrd149/applesauce

export const version = '0.3.4';

// Core event parsing functionality
export function parseEvent(event) {
  return {
    id: event.id,
    pubkey: event.pubkey,
    created_at: event.created_at,
    kind: event.kind,
    tags: event.tags,
    content: event.content,
    sig: event.sig
  };
}

// Tag parsing utilities
export function getTagValue(tags, name) {
  const tag = tags.find(t => t[0] === name);
  return tag ? tag[1] : null;
}

export function getTagValues(tags, name) {
  return tags.filter(t => t[0] === name).map(t => t[1]);
}

export default {
  version,
  parseEvent,
  getTagValue,
  getTagValues
};