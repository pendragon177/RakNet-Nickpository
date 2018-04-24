using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;

public class CustomMsgType
{
    public static short Transform = MsgType.Highest + 1;
};


public class PlayerController : NetworkBehaviour
{
    public float m_linearSpeed = 8.0f;
    public float m_angularSpeed = 3.0f;
	public float m_jumpSpeed = 5.0f;
    public float m_coolDown = 7.0f;
    public float m_stunTime = 2.0f;

    public GameObject bulletPrefab;
    public Transform bulletSpawn;

    private Rigidbody m_rb = null;

    public bool isHoldingFlag = false;
    public bool attackerPoweredUp = false;
    public bool runnerPoweredUp = false;

    public GameObject winText;
    public GameObject loseText;

    bool IsHost()
    {
        return isServer && isLocalPlayer;
    }

    // Use this for initialization
    void Start () {
        m_rb = GetComponent<Rigidbody>();
        //Debug.Log("Start()");
        Vector3 spawnPoint;
        ObjectSpawner.RandomPoint(this.transform.position, 10.0f, out spawnPoint);
        this.transform.position = spawnPoint;

        winText = GameObject.FindGameObjectWithTag("Winner");
        loseText = GameObject.FindGameObjectWithTag("Loser");

        winText.GetComponent<Text>().enabled = false;
        loseText.GetComponent<Text>().enabled = false;
    }

    public override void OnStartAuthority()
    {
        base.OnStartAuthority();
        //Debug.Log("OnStartAuthority()");
    }

    public override void OnStartClient()
    {
        base.OnStartClient();
        //Debug.Log("OnStartClient()");
    }

    public override void OnStartLocalPlayer()
    {
        base.OnStartLocalPlayer();
        //Debug.Log("OnStartLocalPlayer()");
        GetComponent<MeshRenderer>().material.color = new Color(0.0f, 1.0f, 0.0f);
    }

    public override void OnStartServer()
    {
        base.OnStartServer();
        //Debug.Log("OnStartServer()");
    }

    public void Jump()
    {
		Vector3 jumpVelocity = Vector3.up * m_jumpSpeed;
        m_rb.velocity += jumpVelocity;
        //TrailRenderer tr = GetComponent<TrailRenderer>();
        //tr.enabled = true;
    }

    

    // Update is called once per frame
    void Update () {
        m_stunTime += Time.deltaTime;
        if(!isLocalPlayer)
        {
            return;
        }

		if (m_rb.velocity.y < Mathf.Epsilon) {
			//TrailRenderer tr = GetComponent<TrailRenderer>();
			//tr.enabled = false;
		}

        float rotationInput = Input.GetAxis("Horizontal");
        float forwardInput = Input.GetAxis("Vertical");



        if(attackerPoweredUp == true && m_coolDown < 7.0f)
        {
            m_coolDown += Time.deltaTime;
            m_linearSpeed = 12;
        }
        else
        {
            m_linearSpeed = 8;
            attackerPoweredUp = false;
        }

        if (runnerPoweredUp == true && m_coolDown < 7.0f)
        {
            m_coolDown += Time.deltaTime;
        }
        else
        {
            runnerPoweredUp = false;
        }


        if (Input.GetKeyDown(KeyCode.Space) && runnerPoweredUp == true)
        {
            CmdJump();
        }

        if(Input.GetKeyDown(KeyCode.LeftShift) && isHoldingFlag == false)
        {
            CmdFire();
        }

        float yVelocity = m_rb.velocity.y;

        if (m_stunTime > 3.0f)
        {
            Vector3 linearVelocity = this.transform.forward * (forwardInput * m_linearSpeed);

            linearVelocity.y = yVelocity;
            m_rb.velocity = linearVelocity;

            Vector3 angularVelocity = this.transform.up * (rotationInput * m_angularSpeed);
            m_rb.angularVelocity = angularVelocity;
        }


        //End the Game
        float gameTime = GameObject.Find("CTFGameManager").GetComponent<CTFGameManager>().currentTime;

        if(gameTime <= 0 && isHoldingFlag == true && loseText.GetComponent<Text>().enabled == true)
        {
            //Debug.Log("Vicotory in this colour.");
            winText.GetComponent<Text>().enabled = true;
            
        }

        if(gameTime <= 0 && isHoldingFlag == false && winText.GetComponent<Text>().enabled == false)
        {
            //Debug.Log("You lose.");
            loseText.GetComponent<Text>().enabled = true;
        }


    }

    void OnCollisionEnter(Collision collision)
    {
        if (collision.gameObject.tag == "Bullet" && isHoldingFlag == true)
        {
            Debug.Log("I was shot while holding the flag!");
            m_stunTime = 0.0f;
            CmdDropFlag();
            RpcSwapFlagHeld();
        }
        else if (collision.gameObject.tag == "Bullet")
        {
            Debug.Log("I was shot!");
            //CmdDropFlag();
            //CmdChangeFlagState();
        }
    }

    void OnTriggerEnter(Collider other)
    {
        if (other.gameObject.tag == "Flag" && isHoldingFlag == false)
        {
            Debug.Log("I picked up the flag!");

            RpcSwapFlagHeld();
           // RpcSwapFlagHeld();
        }

        if(other.gameObject.tag == "RedPowerup" && isHoldingFlag == false && attackerPoweredUp == false)
        {
            Destroy(other.gameObject);
            attackerPoweredUp = true;
            m_coolDown = 0.0f;
        }

        if (other.gameObject.tag == "BluePowerup" && isHoldingFlag == true && runnerPoweredUp == false)
        {
            Destroy(other.gameObject);
            runnerPoweredUp = true;
            m_coolDown = 0.0f;
        }
    }

    public void swapFlagHeld()
    {
        isHoldingFlag = !isHoldingFlag;
    }

    /*public void checkScore()
    {
        var objects = GameObject.FindGameObjectsWithTag("Player");
        float[] highScore;
        highScore = new float[objects.Length];
        for (int i = 0; i < objects.Length; i++)
        {
            highScore[i] = objects[i].GetComponent<Score>().currentPoints;
            Debug.Log(highScore);
        }
        
    }*/

    /// <summary>
    /// Commands and RPCs
    /// </summary>


    [ClientRpc]
    public void RpcJump()
    {
        Jump();
    }

    [Command]
    public void CmdJump()
    {
        Jump();
        RpcJump();
    }


    [ClientRpc]
    public void RpcSwapFlagHeld()
    {
        swapFlagHeld();
    }

    [Command]
    public void CmdSwapFlagHeld()
    {
        swapFlagHeld();
        //RpcSwapFlagHeld();
    }



    [Command]
    void CmdFire()
    {
        // Create the Bullet from the Bullet Prefab
        var bullet = (GameObject)Instantiate(
            bulletPrefab,
            bulletSpawn.position,
            bulletSpawn.rotation);

        // Add velocity to the bullet
        bullet.GetComponent<Rigidbody>().velocity = bullet.transform.forward * 20;

        NetworkServer.Spawn(bullet);

        // Destroy the bullet after 2 seconds
        Destroy(bullet, 2.0f);
    }

    [Command]
    void CmdDropFlag()
    {
        GameObject.FindGameObjectWithTag("Flag").GetComponent<Flag>().doBoth();
    }

}
