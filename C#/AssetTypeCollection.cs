// Created by Dennis 'DeeFeS' Schmidt 2020
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace DFSSolutions.AssetVariables
{
    public class AssetTypeCollection<T> : ScriptableObject
    {
        [SerializeField] private List<T> _Collection = new List<T>();
        public List<T> Collection => _Collection;
        public T this[int idx] => _Collection[idx];

        public int Count => _Collection.Count;
        public T First => _Collection[0];
        public T Last => _Collection[_Collection.Count - 1];
        public T GetRandom() => _Collection[Random.Range(0, _Collection.Count)];
        public T GetValid(int idx) => _Collection[idx % _Collection.Count];

        public void Add(T item)
        {
#if UNITY_EDITOR
            if (_Collection.Contains(item))
                Debug.LogWarning($"{name}: '{item}' added again!");
#endif
            _Collection.Add(item);
        }

        public void AddAt(T item, int idx)
        {
#if UNITY_EDITOR
            if (idx >= _Collection.Count)
            {
                Debug.LogError($"{name}: Collection not big enough. '{item}' can't be added at index: {idx}!");
                return;
            }
            if (_Collection[idx].Equals(item))
                Debug.Log($"{name}: '{item}' already at position!");
            if (_Collection.Contains(item))
                Debug.LogWarning($"{name}: '{item}' added again!");
#endif

            _Collection[idx] = item;
        }

        public void Remove(T item)
        {
#if UNITY_EDITOR
            if (!_Collection.Contains(item))
                Debug.LogWarning($"{name}: '{item}' is not in the collection!");
#endif
            _Collection.Remove(item);
        }

        public void RemoveAt(T item, int idx)
        {
#if UNITY_EDITOR
            if (!_Collection.Contains(item))
                Debug.LogWarning($"{name}: '{item}' is not in the collection!");
            if (!_Collection[idx].Equals(item))
            {
                Debug.LogError($"{name}: '{item}' is not at the specified position!");
                return;
            }
#endif

            _Collection[idx] = default;
        }

        public void Clear() => _Collection.Clear();
    }
}