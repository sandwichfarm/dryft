// applesauce-lists v0.3.4
// Placeholder for Tungsten
// Full library: https://github.com/hzrd149/applesauce

export const version = '0.3.4';

// List event kinds
export const LIST_KINDS = {
  MUTE: 10000,
  BOOKMARKS: 10003,
  COMMUNITIES: 10004,
  PUBLIC_CHATS: 10005,
  BLOCKED_RELAYS: 10006,
  SEARCH_RELAYS: 10007,
  INTERESTS: 10015,
  EMOJIS: 10030
};

// Parse list event
export function parseList(event) {
  if (!Object.values(LIST_KINDS).includes(event.kind)) {
    throw new Error('Not a list event');
  }
  
  const items = [];
  const privateTags = [];
  
  // Parse tags
  for (const tag of event.tags) {
    if (tag[0] === 'p' || tag[0] === 'e' || tag[0] === 't' || tag[0] === 'a') {
      items.push({
        type: tag[0],
        value: tag[1],
        relay: tag[2],
        petname: tag[3]
      });
    }
  }
  
  // TODO: Parse encrypted content for private items
  
  return {
    kind: event.kind,
    items,
    privateTags,
    created_at: event.created_at
  };
}

// Create list event
export function createList(kind, items, privateItems = []) {
  const tags = [];
  
  // Add public items as tags
  for (const item of items) {
    const tag = [item.type, item.value];
    if (item.relay) tag.push(item.relay);
    if (item.petname) tag.push(item.petname);
    tags.push(tag);
  }
  
  // TODO: Encrypt private items and add to content
  
  return {
    kind,
    tags,
    content: '',
    created_at: Math.floor(Date.now() / 1000)
  };
}

export default {
  version,
  LIST_KINDS,
  parseList,
  createList
};