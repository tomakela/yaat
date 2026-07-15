export async function loadGameData() {
  const [verbsData, inventoryDefs, rooms] = await Promise.all([
    fetch('game/data/verbs.json').then(response => response.json()),
    fetch('game/data/inventory.json').then(response => response.json()),
    fetch('game/data/rooms.json').then(response => response.json()),
  ]);

  return {
    verbs: verbsData.verbs,
    verbLabels: verbsData.verbLabels,
    inventoryDefs,
    rooms,
  };
}
