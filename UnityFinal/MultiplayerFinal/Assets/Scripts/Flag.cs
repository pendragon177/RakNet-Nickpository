using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;

public class Flag : NetworkBehaviour {

    public GameObject flagSparks;
    private ParticleSystem ps;

    enum State
	{
		Available,
		Possessed
	};

	[SyncVar]
	State m_state;

	// Use this for initialization
	void Start () {

        ps = flagSparks.GetComponent<ParticleSystem>();
        
        //Vector3 spawnPoint;
        //ObjectSpawner.RandomPoint(this.transform.position, 10.0f, out spawnPoint);
        //this.transform.position = spawnPoint;
        //GetComponent<MeshRenderer> ().enabled = false;
        m_state = State.Available;
        
    }

    [ClientRpc]
    public void RpcPickUpFlag(GameObject player)
    {
        AttachFlagToGameObject(player);
    }

    public void AttachFlagToGameObject(GameObject obj)
    {
        this.transform.parent = obj.transform;
    }


    [ClientRpc]
    public void RpcDropFlag()
    {
        DetatchFlagFromGameObject();
    }

    public void DetatchFlagFromGameObject()
    {
        this.transform.position = this.transform.position - this.transform.forward *2;
        this.transform.parent = null;
    }

    public void doBoth()
    {
        Debug.Log("doBoth() happened.");
        RpcDropFlag();
        DetatchFlagFromGameObject();
        m_state = State.Available;
    }


    void OnTriggerEnter(Collider other)
    {
        if(m_state == State.Possessed || !isServer || other.tag != "Player")
        {
            return;
        }

        m_state = State.Possessed;
        AttachFlagToGameObject(other.gameObject);
        RpcPickUpFlag(other.gameObject);
    }

    // Update is called once per frame
    void Update () {
        var emission = ps.emission;
        float gameTime = GameObject.Find("CTFGameManager").GetComponent<CTFGameManager>().currentTime;

        if (m_state == State.Available)
        {
            emission.enabled = true;
        }
        else
        {
            emission.enabled = false;
        }

        if(m_state == State.Possessed && gameTime > 0)
        {
            GetComponentInParent<Score>().AddScore();
        }

        if(gameTime <= 0)
        {
            if(this.transform.parent == null)
            {
                //Debug.Log("There are no winners.");
            }
        }
	}
}
